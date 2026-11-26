#!/usr/bin/env python3
"""End-to-end checks using small, independently constructed CCF v1 files.

Run through ``make test``. Each invocation gets a fresh working directory so
the tests exercise the public CLI and its JSON reports without touching repo
outputs or requiring cc_con to be installed.
"""

import hashlib
import json
import math
import random
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


BINARY = Path(__file__).resolve().parents[1] / "bin" / "cp_algo"
UINT64_MAX = (1 << 64) - 1


def vbyte(value):
    """Encode a nonnegative integer in CCF's low-seven-bits-first format."""
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return result


def write_ccf(path, n, cliques):
    """Write a valid cover for the graph induced by the supplied cliques."""
    edges = set()
    records = []
    for clique in cliques:
        assert clique == sorted(set(clique))
        assert all(0 <= vertex < n for vertex in clique)
        record = vbyte(len(clique))
        previous = 0
        for position, vertex in enumerate(clique):
            record += vbyte(vertex if position == 0 else vertex - previous)
            previous = vertex
        records.append(record)
        for i, left in enumerate(clique):
            for right in clique[i + 1 :]:
                edges.add((left, right))

    p = len(cliques)
    payload_start = 160 + 8 * (p + 1)
    offsets = [payload_start]
    for record in records:
        offsets.append(offsets[-1] + len(record))
    file_bytes = offsets[-1]

    graph_stream = bytearray(b"CC-CON-CANONICAL-GRAPH-v1\0")
    graph_stream += struct.pack("<QQ", n, len(edges))
    for left, right in sorted(edges):
        graph_stream += struct.pack("<QQ", left, right)
    graph_hash = hashlib.sha256(graph_stream).digest()

    output = bytearray(file_bytes)
    output[:8] = b"CCF\0\r\n\x1a\n"
    struct.pack_into("<IIII", output, 8, 1, 160, 1, 0)
    struct.pack_into(
        "<QQQQQQQQQ",
        output,
        24,
        n,
        len(edges),
        p,
        sum(map(len, cliques)),
        160,
        p + 1,
        payload_start,
        file_bytes - payload_start,
        file_bytes,
    )
    output[96:128] = graph_hash
    for i, offset in enumerate(offsets):
        struct.pack_into("<Q", output, 160 + 8 * i, offset)
    output[payload_start:] = b"".join(records)
    output[128:160] = hashlib.sha256(output).digest()
    path.write_bytes(output)
    return graph_hash.hex()


def optimum_matched_weight(n, cliques, weights):
    """Exact small-graph oracle, independent of the cover algorithm."""
    neighbors = [0] * n
    for clique in cliques:
        for i, u in enumerate(clique):
            for v in clique[i + 1 :]:
                neighbors[u] |= 1 << v
                neighbors[v] |= 1 << u

    cache = {0: 0}

    def solve(remaining):
        if remaining in cache:
            return cache[remaining]
        u_bit = remaining & -remaining
        u = u_bit.bit_length() - 1
        rest = remaining ^ u_bit
        best = solve(rest)
        candidates = neighbors[u] & rest
        while candidates:
            v_bit = candidates & -candidates
            v = v_bit.bit_length() - 1
            best = max(best, weights[u] + weights[v] + solve(rest ^ v_bit))
            candidates ^= v_bit
        cache[remaining] = best
        return best

    return solve((1 << n) - 1)


class CliIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cp_algo_test_")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.graph = self.directory / "paths-and-isolate.ccf"
        # 0-1, 0-2, 1-3 is a four-vertex path. 4-5 is a separate edge; 6
        # is isolated. The first greedy edge is 0-1, which can be augmented
        # through free vertices 2 and 3.
        self.fingerprint = write_ccf(
            self.graph, 7, [[0, 1], [0, 2], [1, 3], [4, 5]]
        )

    def config(self, name, contents):
        path = self.directory / name
        path.write_text(contents, encoding="utf-8")
        return path

    def command(self, algorithm, graph=None, config=None):
        args = [str(BINARY), algorithm, str(graph or self.graph)]
        if config is not None:
            args += ["--config", str(config)]
        return args

    def run_report(self, algorithm, graph=None, config=None):
        completed = subprocess.run(
            self.command(algorithm, graph, config),
            cwd=self.directory,
            text=True,
            capture_output=True,
            check=True,
        )
        report_path = Path(completed.stdout.strip())
        self.assertEqual(completed.stderr, "")
        self.assertEqual(report_path.parent.resolve(), self.directory.resolve())
        self.assertTrue(report_path.is_file())
        report = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual(report["algorithm"], algorithm)
        self.assertEqual(report["input"]["graph_fingerprint_sha256"], self.fingerprint)
        self.assertEqual(report["input"]["n"], 7)
        self.assertEqual(report["input"]["m"], 4)
        self.assertEqual(report["input"]["num_cliques"], 4)
        self.assertEqual(report["input"]["cover_size"], 8)
        self.assertEqual(report["input"]["ccf_bytes"], self.graph.stat().st_size)
        self.assertIsNone(report["slurm_job_id"])
        self.assertGreater(report["memory"]["process_peak_rss_bytes"], 0)
        times = report["timing_seconds"]
        for key in ("ccf_load", "algorithm"):
            self.assertIn(key, times)
        for value in times.values():
            self.assertTrue(math.isfinite(value))
            self.assertGreaterEqual(value, 0)
        if report["cover_mode"] == "plain":
            self.assertIn("cover_decode", times)
        else:
            self.assertNotIn("cover_decode", times)
        return report_path, report

    def mvm_report(self, graph, config):
        completed = subprocess.run(
            self.command("mvm", graph, config), cwd=self.directory,
            text=True, capture_output=True, check=True,
        )
        self.assertEqual(completed.stderr, "")
        return json.loads(Path(completed.stdout.strip()).read_text())

    def test_matching_variants_in_both_modes(self):
        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        maximal = self.config("maximal.toml", 'matching_variant = "maximal"\n')
        encoded_maximal = self.config(
            "encoded-maximal.toml",
            'cover_mode = "encoded"\nmatching_variant = "maximal"\n',
        )
        cases = [
            (None, "plain", "three_halves", 3),
            (encoded, "encoded", "three_halves", 3),
            (maximal, "plain", "maximal", 2),
            (encoded_maximal, "encoded", "maximal", 2),
        ]
        for config, mode, variant, expected in cases:
            with self.subTest(mode=mode, variant=variant):
                _, report = self.run_report("matching", config=config)
                self.assertEqual(report["cover_mode"], mode)
                self.assertEqual(report["parameters"]["matching_variant"], variant)
                self.assertEqual(report["result"], {"matching_size": expected})

    def test_bfs_and_dfs_visit_all_components_in_both_modes(self):
        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        for algorithm in ("bfs", "dfs"):
            for config, mode in ((None, "plain"), (encoded, "encoded")):
                with self.subTest(algorithm=algorithm, mode=mode):
                    _, report = self.run_report(algorithm, config=config)
                    self.assertEqual(report["cover_mode"], mode)
                    self.assertEqual(report["result"], {"connected_components": 3})

    def test_wvc_explicit_and_generated_weights_in_both_modes(self):
        weights = self.directory / "weights.txt"
        weights.write_text("2\n7\n5\n3\n9\n4\n11\n", encoding="utf-8")
        explicit = self.config("weights.toml", 'weights_file = "weights.txt"\n')
        encoded_explicit = self.config(
            "encoded-weights.toml",
            'cover_mode = "encoded"\nweights_file = "weights.txt"\n',
        )
        for config, mode in ((explicit, "plain"), (encoded_explicit, "encoded")):
            with self.subTest(mode=mode, weights="file"):
                _, report = self.run_report("wvc", config=config)
                # Tight vertices are 0, 3, and 5: 2 + 3 + 4 = 9.
                self.assertEqual(report["result"], {"selected_weight_sum": "9"})
                self.assertEqual(report["parameters"]["weights_file"], str(weights))
                self.assertIn("weight_preparation", report["timing_seconds"])

        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        _, plain_report = self.run_report("wvc")
        _, encoded_report = self.run_report("wvc", config=encoded)
        self.assertEqual(plain_report["result"], encoded_report["result"])
        total = int(plain_report["result"]["selected_weight_sum"])
        self.assertGreater(total, 0)
        self.assertLessEqual(total, 7 ** 3)
        self.assertEqual(plain_report["parameters"]["weight_seed"], 19)

    def test_wvc_sum_can_exceed_uint64(self):
        graph = self.directory / "triangle.ccf"
        write_ccf(graph, 3, [[0, 1, 2]])
        weights = self.directory / "large-weights.txt"
        weights.write_text((str(UINT64_MAX) + "\n") * 3, encoding="utf-8")
        for mode in ("plain", "encoded"):
            config = self.config(
                mode + "-large.toml",
                f'cover_mode = "{mode}"\nweights_file = "large-weights.txt"\n',
            )
            completed = subprocess.run(
                self.command("wvc", graph, config),
                cwd=self.directory,
                text=True,
                capture_output=True,
                check=True,
            )
            result = json.loads(Path(completed.stdout.strip()).read_text())
            self.assertEqual(
                result["result"]["selected_weight_sum"], str(2 * UINT64_MAX)
            )

    def test_mvm_increasing_paths_and_report(self):
        cases = [
            # A triangle replaces the light matched endpoint by its free one.
            ("length-two", 3, [[0, 1, 2]], [1, 10, 10], 20),
            # The free endpoint 4 is lighter than its immediate replacement
            # 2, but heavier than terminal 0 across two matching edges.
            ("length-four", 5,
             [[0, 1], [2, 3], [1, 2], [3, 4]],
             [1, 100, 100, 100, 50], 350),
        ]
        for name, n, cliques, weights, expected in cases:
            with self.subTest(name=name):
                graph = self.directory / f"mvm-{name}.ccf"
                write_ccf(graph, n, cliques)
                weight_file = self.directory / f"mvm-{name}.txt"
                weight_file.write_text("\n".join(map(str, weights)) + "\n")
                reports = []
                for mode in ("plain", "encoded"):
                    config = self.config(
                        f"mvm-{name}-{mode}.toml",
                        f'cover_mode = "{mode}"\n'
                        f'weights_file = "{weight_file.name}"\n',
                    )
                    report = self.mvm_report(graph, config)
                    self.assertEqual(report["cover_mode"], mode)
                    self.assertEqual(report["result"],
                                     {"matched_vertex_weight_sum": str(expected)})
                    self.assertEqual(report["parameters"]["mvm_mode"], "bounded")
                    self.assertEqual(report["parameters"]["mvm_round_limit"], 30)
                    self.assertGreaterEqual(report["mvm_execution"]["rounds_with_flip"], 1)
                    self.assertTrue(report["mvm_execution"]["early_exit"])
                    self.assertIn("algorithm", report["timing_seconds"])
                    self.assertEqual("cover_decode" in report["timing_seconds"],
                                     mode == "plain")
                    self.assertIn("weight_preparation", report["timing_seconds"])
                    reports.append(report)
                self.assertEqual(reports[0]["mvm_execution"],
                                 reports[1]["mvm_execution"])

    def test_mvm_multi_round_rebuilds_match_in_both_modes(self):
        # Two packing rounds change the matching and rebuild beta. The last
        # round finds no positive gain; stale records would change the answer.
        cliques = [
            [0, 1, 4, 10, 14, 15], [1, 3, 7, 17], [6, 14],
            [7, 13, 18], [0, 5, 9, 13, 16, 20], [1, 2, 3, 4, 15, 20],
        ]
        weights = [100, 10, 43, 41, 31, 97, 66, 58, 61, 28, 13,
                   17, 62, 60, 64, 57, 73, 8, 80, 49, 97]
        graph = self.directory / "mvm-multi-round.ccf"
        write_ccf(graph, 21, cliques)
        weight_file = self.directory / "mvm-multi-round.txt"
        weight_file.write_text("\n".join(map(str, weights)) + "\n")
        for policy in ("bounded", "until_no_improvement"):
            reports = []
            for mode in ("plain", "encoded"):
                config = self.config(
                    f"mvm-multi-round-{policy}-{mode}.toml",
                    f'cover_mode = "{mode}"\n'
                    f'weights_file = "{weight_file.name}"\n'
                    f'mvm_mode = "{policy}"\n',
                )
                reports.append(self.mvm_report(graph, config))
            with self.subTest(policy=policy):
                self.assertEqual(reports[0]["result"],
                                 {"matched_vertex_weight_sum": "918"})
                self.assertEqual(reports[0]["result"], reports[1]["result"])
                self.assertEqual(reports[0]["mvm_execution"],
                                 reports[1]["mvm_execution"])
                self.assertEqual(reports[0]["mvm_execution"]["rounds_with_flip"], 2)
                self.assertEqual(reports[0]["mvm_execution"]["rounds_attempted"], 3)
                self.assertEqual(reports[0]["mvm_execution"]["stop_reason"],
                                 "no_positive_gain")

    def test_mvm_approximation_on_small_random_covers(self):
        rng = random.Random(86029)
        for trial in range(40):
            n = rng.randint(4, 9)
            cliques = []
            for _ in range(rng.randint(1, 10)):
                size = rng.randint(2, min(n, 5))
                cliques.append(sorted(rng.sample(range(n), size)))
            weights = [rng.randint(1, 30) for _ in range(n)]
            optimum = optimum_matched_weight(n, cliques, weights)
            graph = self.directory / f"mvm-random-{trial}.ccf"
            write_ccf(graph, n, cliques)
            weight_file = self.directory / f"mvm-random-{trial}.txt"
            weight_file.write_text("\n".join(map(str, weights)) + "\n")
            for mode in ("bounded", "until_no_improvement"):
                reports = []
                for cover_mode in ("plain", "encoded"):
                    config = self.config(
                        f"mvm-random-{trial}-{mode}-{cover_mode}.toml",
                        f'cover_mode = "{cover_mode}"\n'
                        f'weights_file = "{weight_file.name}"\n'
                        f'mvm_mode = "{mode}"\n',
                    )
                    reports.append(self.mvm_report(graph, config))
                report = reports[0]
                value = int(report["result"]["matched_vertex_weight_sum"])
                execution = report["mvm_execution"]
                with self.subTest(trial=trial, mode=mode):
                    self.assertEqual(report["result"], reports[1]["result"])
                    self.assertEqual(execution, reports[1]["mvm_execution"])
                    self.assertLessEqual(value, optimum)
                    self.assertGreaterEqual(151 * value, 100 * optimum)
                    self.assertGreaterEqual(execution["rounds_attempted"],
                                            execution["rounds_with_flip"])
                    if mode == "until_no_improvement":
                        self.assertGreaterEqual(3 * value, 2 * optimum)
                        self.assertNotEqual(execution["stop_reason"], "round_limit")

    def test_mvm_full_width_sum_and_config_rejection(self):
        graph = self.directory / "mvm-large.ccf"
        write_ccf(graph, 3, [[0, 1, 2]])
        weights = self.directory / "mvm-large.txt"
        weights.write_text(f"{UINT64_MAX}\n{UINT64_MAX}\n1\n")
        reports = []
        for mode in ("plain", "encoded"):
            config = self.config(
                f"mvm-large-{mode}.toml",
                f'cover_mode = "{mode}"\nweights_file = "mvm-large.txt"\n',
            )
            report = self.mvm_report(graph, config)
            self.assertEqual(report["result"],
                             {"matched_vertex_weight_sum": str(2 * UINT64_MAX)})
            reports.append(report)
        self.assertEqual(reports[0]["mvm_execution"],
                         reports[1]["mvm_execution"])

        bad_configs = [
            ('mvm_epsilon = 0\n', "positive finite"),
            ('mvm_mode = "other"\n', "mvm_mode"),
        ]
        for index, (contents, expected) in enumerate(bad_configs):
            invalid = self.config(f"mvm-invalid-{index}.toml", contents)
            completed = subprocess.run(
                self.command("mvm", graph, invalid), cwd=self.directory,
                text=True, capture_output=True,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn(expected, completed.stderr)

    def test_multibyte_vertex_ids_and_clique_labels(self):
        graph = self.directory / "large-ids.ccf"
        # Vertex 128 takes multiple CCF vbytes; clique label 129 needs more
        # bits in packed incidence lists. Only {0,128,257} is non-isolated.
        cliques = [[0, 128] for _ in range(129)] + [[128, 257]]
        write_ccf(graph, 260, cliques)
        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        for algorithm in ("bfs", "dfs"):
            for config in (None, encoded):
                with self.subTest(algorithm=algorithm, encoded=config is not None):
                    completed = subprocess.run(
                        self.command(algorithm, graph, config),
                        cwd=self.directory,
                        text=True,
                        capture_output=True,
                        check=True,
                    )
                    report = json.loads(Path(completed.stdout.strip()).read_text())
                    self.assertEqual(
                        report["result"], {"connected_components": 258}
                    )

        # MVM uses the same wide vertex IDs and packs clique labels in L_v.
        plain = self.mvm_report(graph, None)
        packed = self.mvm_report(graph, encoded)
        self.assertEqual(plain["result"], packed["result"])
        self.assertEqual(plain["mvm_execution"], packed["mvm_execution"])

    def test_mvm_multibyte_clique_size_and_initial_perfect_matching(self):
        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        large_clique = self.directory / "mvm-large-clique.ccf"
        write_ccf(large_clique, 129, [list(range(129))])
        plain = self.mvm_report(large_clique, None)
        packed = self.mvm_report(large_clique, encoded)
        self.assertEqual(plain["result"], packed["result"])
        self.assertEqual(plain["mvm_execution"], packed["mvm_execution"])

        perfect = self.directory / "mvm-perfect.ccf"
        write_ccf(perfect, 2, [[0, 1]])
        plain = self.mvm_report(perfect, None)
        packed = self.mvm_report(perfect, encoded)
        self.assertEqual(plain["result"], packed["result"])
        self.assertEqual(plain["mvm_execution"], packed["mvm_execution"])
        self.assertEqual(packed["mvm_execution"], {
            "rounds_attempted": 1,
            "rounds_with_flip": 0,
            "stop_reason": "all_vertices_matched",
            "early_exit": True,
        })

    def test_encoded_results_match_plain_across_cover_shapes(self):
        # These covers cross several clique-label bit widths and have
        # overlapping cliques, repeated discoveries, and isolated vertices.
        rng = random.Random(271828)
        shapes = [(6, 0), (3, 1), (5, 3), (9, 7),
                  (13, 15), (17, 17), (31, 33)]
        encoded = self.config("encoded.toml", 'cover_mode = "encoded"\n')
        maximal = self.config("maximal.toml", 'matching_variant = "maximal"\n')
        encoded_maximal = self.config(
            "encoded-maximal.toml",
            'cover_mode = "encoded"\nmatching_variant = "maximal"\n',
        )

        def result(algorithm, graph, config):
            completed = subprocess.run(
                self.command(algorithm, graph, config),
                cwd=self.directory,
                text=True,
                capture_output=True,
                check=True,
            )
            return json.loads(Path(completed.stdout.strip()).read_text())["result"]

        for n, p in shapes:
            cliques = []
            for _ in range(p):
                size = rng.randint(2, min(n, 6))
                cliques.append(sorted(rng.sample(range(n), size)))
            graph = self.directory / f"shape-{n}-{p}.ccf"
            write_ccf(graph, n, cliques)

            parent = list(range(n))

            def root(v):
                while parent[v] != v:
                    v = parent[v]
                return v

            for clique in cliques:
                for v in clique[1:]:
                    parent[root(v)] = root(clique[0])
            components = len({root(v) for v in range(n)})

            for algorithm in ("matching", "bfs", "dfs", "wvc", "mvm"):
                with self.subTest(n=n, p=p, algorithm=algorithm):
                    plain = result(algorithm, graph, None)
                    packed = result(algorithm, graph, encoded)
                    self.assertEqual(packed, plain)
                    if algorithm in ("bfs", "dfs"):
                        self.assertEqual(plain, {"connected_components": components})
                    if algorithm == "mvm":
                        self.assertEqual(
                            self.mvm_report(graph, None)["mvm_execution"],
                            self.mvm_report(graph, encoded)["mvm_execution"],
                        )

            with self.subTest(n=n, p=p, algorithm="maximal"):
                self.assertEqual(
                    result("matching", graph, encoded_maximal),
                    result("matching", graph, maximal),
                )

    def test_bad_inputs_do_not_publish_reports(self):
        invalid_mode = self.config("bad-mode.toml", 'cover_mode = "unknown"\n')
        wrong_option = self.config("wrong-option.toml", 'matching_variant = "maximal"\n')
        short_weights = self.directory / "short-weights.txt"
        short_weights.write_text("1\n2\n", encoding="utf-8")
        bad_weights = self.config("bad-weights.toml", 'weights_file = "short-weights.txt"\n')
        malformed = self.directory / "malformed.ccf"
        malformed.write_bytes(b"not a CCF")
        cases = [
            ("bfs", self.graph, invalid_mode, "cover_mode"),
            ("bfs", self.graph, wrong_option, "matching_variant"),
            ("wvc", self.graph, bad_weights, "exactly n"),
            ("matching", malformed, None, "invalid CCF"),
        ]
        for algorithm, graph, config, error_text in cases:
            with self.subTest(error=error_text):
                completed = subprocess.run(
                    self.command(algorithm, graph, config),
                    cwd=self.directory,
                    text=True,
                    capture_output=True,
                )
                self.assertNotEqual(completed.returncode, 0)
                self.assertIn(error_text, completed.stderr)
        self.assertEqual(list(self.directory.glob("*.json")), [])
        self.assertEqual(list(self.directory.glob("*.claim")), [])

    def test_existing_claim_tries_next_report_name(self):
        # A name collision may set EEXIST when the claim is a regular file.
        # Shared filesystems can also surface EEXIST during concurrent claims.
        claim = self.directory / "matching-paths-and-isolate-plain.json.claim"
        claim.write_text("occupied", encoding="utf-8")
        path, report = self.run_report("matching")
        self.assertEqual(path.name, "matching-paths-and-isolate-plain-2.json")
        self.assertEqual(report["result"], {"matching_size": 3})
        self.assertEqual(claim.read_text(encoding="utf-8"), "occupied")

    def test_concurrent_runs_publish_distinct_complete_reports(self):
        # Repetitions belong to the calling script/Slurm, not the binary. All
        # processes use the same CCF and cwd to catch report-name races.
        processes = [
            subprocess.Popen(
                self.command("matching"),
                cwd=self.directory,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            for _ in range(8)
        ]
        paths = []
        for process in processes:
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 0, stderr)
            self.assertEqual(stderr, "")
            paths.append(Path(stdout.strip()))
        self.assertEqual(len(set(paths)), len(paths))
        for path in paths:
            self.assertEqual(path.parent.resolve(), self.directory.resolve())
            self.assertEqual(
                json.loads(path.read_text(encoding="utf-8"))["result"],
                {"matching_size": 3},
            )
        self.assertEqual(list(self.directory.glob("*.claim")), [])
        self.assertEqual(list(self.directory.glob("*.writing")), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
