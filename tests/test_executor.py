#!/usr/bin/env python3
"""
Widgeteer Test Executor

Execute tests defined in JSON files against a Widgeteer-enabled Qt application.
"""

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from widgeteer_client import WidgeteerClient, Response


@dataclass
class TestResult:
    """Result of a single test."""
    name: str
    passed: bool
    duration_ms: float
    error: str | None = None
    steps_completed: int = 0
    steps_total: int = 0


@dataclass
class TestSuiteResult:
    """Result of a test suite."""
    name: str
    results: list[TestResult] = field(default_factory=list)

    @property
    def passed(self) -> int:
        return sum(1 for r in self.results if r.passed)

    @property
    def failed(self) -> int:
        return sum(1 for r in self.results if not r.passed)

    @property
    def total(self) -> int:
        return len(self.results)


class TestExecutor:
    """Execute tests from JSON files."""

    def __init__(self, client: WidgeteerClient, verbose: bool = False):
        self.client = client
        self.verbose = verbose

    def log(self, message: str):
        """Log a message if verbose mode is enabled."""
        if self.verbose:
            print(f"  {message}")

    def execute_step(self, step: dict) -> Response:
        """Execute a single test step."""
        command = step.get("command")
        params = step.get("params", {})

        self.log(f"  -> {command}: {params.get('target', params)}")

        return self.client.command(command, params)

    def check_assertion(self, assertion: dict) -> tuple[bool, str | None]:
        """Check an assertion."""
        target = assertion.get("target")
        property_name = assertion.get("property")
        operator = assertion.get("operator", "==")
        expected = assertion.get("value")

        resp = self.client.assert_property(target, property_name, operator, expected)

        if not resp.success:
            return False, resp.error

        # The response has "result" nested, so check both locations for compatibility
        result = resp.data.get("result", resp.data)
        passed = result.get("passed", False)
        if not passed:
            actual = result.get("actual")
            return False, f"Expected {property_name} {operator} {expected}, got {actual}"

        return True, None

    def run_test(self, test: dict) -> TestResult:
        """Run a single test."""
        name = test.get("name", "unnamed")
        steps = test.get("steps", [])
        assertions = test.get("assertions", [])

        start_time = time.time()
        steps_completed = 0
        error = None

        self.log(f"Running test: {name}")

        # Execute steps
        for i, step in enumerate(steps):
            resp = self.execute_step(step)

            if not resp.success:
                error = f"Step {i+1} failed: {resp.error}"
                break

            steps_completed += 1

            # Optional delay between steps
            delay = step.get("delay_ms")
            if delay:
                time.sleep(delay / 1000.0)

        # Run assertions if all steps passed
        if error is None:
            for assertion in assertions:
                passed, err = self.check_assertion(assertion)
                if not passed:
                    error = f"Assertion failed: {err}"
                    break

        duration_ms = (time.time() - start_time) * 1000

        return TestResult(
            name=name,
            passed=error is None,
            duration_ms=duration_ms,
            error=error,
            steps_completed=steps_completed,
            steps_total=len(steps)
        )

    def run_suite(self, suite: dict) -> TestSuiteResult:
        """Run a test suite."""
        name = suite.get("name", "unnamed")
        tests = suite.get("tests", [])
        setup = suite.get("setup", [])
        teardown = suite.get("teardown", [])

        result = TestSuiteResult(name=name)

        print(f"\n{'='*60}")
        print(f"Test Suite: {name}")
        print('='*60)

        # Run setup
        if setup:
            self.log("Running setup...")
            for step in setup:
                resp = self.execute_step(step)
                if not resp.success:
                    print(f"Setup failed: {resp.error}")
                    return result

        # Run tests
        for test in tests:
            test_result = self.run_test(test)
            result.results.append(test_result)

            status = "✓ PASS" if test_result.passed else "✗ FAIL"
            print(f"  {status} {test_result.name} ({test_result.duration_ms:.1f}ms)")
            if test_result.error:
                print(f"       Error: {test_result.error}")

        # Run teardown
        if teardown:
            self.log("Running teardown...")
            for step in teardown:
                self.execute_step(step)

        return result

    def run_file(self, path: Path) -> TestSuiteResult:
        """Run tests from a JSON file."""
        with open(path) as f:
            suite = json.load(f)

        return self.run_suite(suite)


def wait_for_server(client: WidgeteerClient, timeout: float = 10.0) -> bool:
    """Wait for server to become available."""
    start = time.time()
    while time.time() - start < timeout:
        resp = client.health()
        if resp.success:
            return True
        time.sleep(0.5)
    return False


def main():
    parser = argparse.ArgumentParser(description="Widgeteer Test Executor")
    parser.add_argument("test_file", type=Path, nargs="?",
                        help="JSON test file to execute")
    parser.add_argument("--host", default="localhost",
                        help="Widgeteer server host (default: localhost)")
    parser.add_argument("--port", type=int, default=9000,
                        help="Widgeteer server port (default: 9000)")
    parser.add_argument("--app", type=Path,
                        help="Path to application to launch before testing")
    parser.add_argument("--wait", type=float, default=5.0,
                        help="Time to wait for application to start (default: 5s)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose output")
    parser.add_argument("--list-tests", action="store_true",
                        help="List tests in file without running")

    args = parser.parse_args()

    client = WidgeteerClient(host=args.host, port=args.port)
    app_process = None

    # Launch application if specified
    if args.app:
        print(f"Launching application: {args.app}")
        app_process = subprocess.Popen([str(args.app), str(args.port)])
        time.sleep(1)  # Give it time to start

    # Wait for server
    print(f"Connecting to Widgeteer server at {args.host}:{args.port}...")
    if not wait_for_server(client, args.wait):
        print("Error: Server not available")
        if app_process:
            app_process.terminate()
        sys.exit(1)

    print("Connected!")

    try:
        if args.test_file:
            if args.list_tests:
                # List tests only
                with open(args.test_file) as f:
                    suite = json.load(f)
                print(f"\nTests in {args.test_file}:")
                for test in suite.get("tests", []):
                    print(f"  - {test.get('name', 'unnamed')}")
            else:
                # Run tests
                executor = TestExecutor(client, verbose=args.verbose)
                result = executor.run_file(args.test_file)

                print(f"\n{'='*60}")
                print(f"Results: {result.passed}/{result.total} passed")
                print('='*60)

                if result.failed > 0:
                    sys.exit(1)
        else:
            # No test file - just check connection and print tree
            print("\nWidget tree:")
            resp = client.tree(depth=2)
            if resp.success:
                print(json.dumps(resp.data, indent=2)[:1000])
            else:
                print(f"Error: {resp.error}")

    finally:
        if app_process:
            print("\nTerminating application...")
            app_process.terminate()
            app_process.wait(timeout=5)


if __name__ == "__main__":
    main()
