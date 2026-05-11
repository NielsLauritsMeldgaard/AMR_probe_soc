#!/usr/bin/env python3
"""Run RISC-V testbench simulations with test selection via plusargs.

Discovers .mem files in tests directory, runs xsim for each test with +TEST=,
and reports results.
"""

import argparse
import subprocess
import sys
from pathlib import Path
import shutil
import os


def find_xsim() -> str:
	"""Find xsim executable automatically across Vivado versions.
	
	Returns:
		Full path to xsim.bat (Windows) or xsim (Linux)
	
	Raises:
		FileNotFoundError: If xsim cannot be found
	"""
	# Try PATH first
	xsim_path = shutil.which("xsim")
	if xsim_path:
		return xsim_path
	
	# Environment variable hint (XILINX_VIVADO usually points at version dir)
	xilinx_vivado = os.environ.get("XILINX_VIVADO")
	if xilinx_vivado:
		cand = Path(xilinx_vivado) / "bin" / ("xsim.bat" if os.name == "nt" else "xsim")
		if cand.exists():
			return str(cand)
	
	# Search common Vivado installation directories
	for base in [Path("C:/Xilinx/Vivado"), Path("D:/Xilinx/Vivado")]:
		if base.exists():
			version_dirs = sorted(base.glob("20*"), reverse=True)
			for version_dir in version_dirs:
				xsim = version_dir / "bin" / ("xsim.bat" if os.name == "nt" else "xsim")
				if xsim.exists():
					return str(xsim)
	
	# Linux paths
	vivado_root_linux = Path("/opt/Xilinx/Vivado")
	if vivado_root_linux.exists():
		version_dirs = sorted(vivado_root_linux.glob("20*"), reverse=True)
		for version_dir in version_dirs:
			xsim_bin = version_dir / "bin" / "xsim"
			if xsim_bin.exists():
				return str(xsim_bin)
	
	raise FileNotFoundError(
		"xsim not found. Please:\n"
		"  1. Add Vivado bin/ to PATH, or\n"
		"  2. Use --xsim-path to specify location"
	)


def find_tests(root: Path, task_filter: str = None) -> list[tuple[str, Path]]:
	"""Find all test .mem files. Returns list of (test_name, mem_path).
	
	Args:
		root: Root tests directory
		task_filter: Optional task filter (e.g., "task1" finds all task1/*)
	
	Returns:
		List of (test_name, path) tuples, e.g. ("task1/addpos", Path(...))
	"""
	tests = []
	
	if task_filter:
		# Search only in specific task folder(s)
		for task in task_filter if isinstance(task_filter, list) else [task_filter]:
			task_dir = root / task
			if not task_dir.is_dir():
				print(f"Warning: {task} not found", file=sys.stderr)
				continue
			for mem_path in sorted(task_dir.glob("*.mem")):
				test_name = str(mem_path.relative_to(root).with_suffix("")).replace("\\", "/")
				tests.append((test_name, mem_path))
	else:
		# Search all tasks
		for mem_path in sorted(root.rglob("*.mem")):
			test_name = str(mem_path.relative_to(root).with_suffix("")).replace("\\", "/")
			tests.append((test_name, mem_path))
	
	return tests


def run_test(test_name: str, testbench_root: str, xsim_snapshot: str, 
             xsim_path: str = r"C:\Xilinx\Vivado\2023.1\bin\xsim.bat", timeout: int = 60) -> bool:
	"""Run a single test via xsim. Returns True if passed.
	
	Args:
		test_name: Test name like "task1/addpos"
		testbench_root: Path to testbench root (e.g., "RISC_V/RISC_V.sim/sim_1/behav/xsim")
		xsim_snapshot: xsim snapshot name (e.g., "tb_datapath_behav")
		xsim_path: Full path to xsim executable
		timeout: Max seconds to wait
	
	Returns:
		True if "[TB] PASS" found in output, False otherwise
	"""
	print(f"\n{'='*60}")
	print(f"Running: {test_name}")
	print(f"{'='*60}")
	
	# Build xsim command - use shell=True for Windows batch compatibility
	cmd = f'"{xsim_path}" {xsim_snapshot} -R --testplusarg "TEST={test_name}"'
	
	try:
		result = subprocess.run(
			cmd,
			cwd=testbench_root,
			capture_output=True,
			text=True,
			timeout=timeout,
			shell=True,
		)
		
		# Print stdout
		if result.stdout:
			print(result.stdout)
		if result.stderr:
			print(result.stderr, file=sys.stderr)
		
		# Check for PASS
		passed = "[TB] PASS" in result.stdout
		status = "✓ PASSED" if passed else "✗ FAILED"
		print(f"{status}: {test_name}")
		
		return passed
		
	except subprocess.TimeoutExpired:
		print(f"✗ TIMEOUT: {test_name}")
		return False
	except FileNotFoundError:
		print(f"✗ ERROR: xsim not found. Make sure Vivado is in PATH")
		return False
	except Exception as e:
		print(f"✗ ERROR: {e}")
		return False


def main():
	parser = argparse.ArgumentParser(
		description="Run RISC-V testbench with test selection"
	)
	parser.add_argument(
		"--task",
		nargs="*",
		help="Task(s) to run (e.g., task1 or task1 task2). If omitted, runs all.",
	)
	parser.add_argument(
		"--test",
		help="Single test to run (e.g., task1/addpos)",
	)
	parser.add_argument(
		"--testbench-root",
		help="Path to xsim working directory (auto-detected if omitted)",
	)
	parser.add_argument(
		"--test-root",
		type=Path,
		help="Root directory containing test .mem files (default: script directory)",
	)
	parser.add_argument(
		"--snapshot",
		default="tb_datapath_behav",
		help="xsim snapshot name (default: tb_datapath_behav)",
	)
	parser.add_argument(
		"--xsim-path",
		help="Path to xsim executable (auto-detected if omitted)",
	)
	parser.add_argument(
		"--timeout",
		type=int,
		default=60,
		help="Timeout per test in seconds (default: 60)",
	)
	
	args = parser.parse_args()
	
	# Find xsim
	try:
		xsim_path = args.xsim_path or find_xsim()
	except FileNotFoundError as e:
		print(f"ERROR: {e}", file=sys.stderr)
		return 1
	
	# Auto-detect testbench root if not specified
	if not args.testbench_root:
		# Script is in RISC_V/tests, testbench is in RISC_V/RISC_V.sim/sim_1/behav/xsim
		testbench_root = Path(__file__).resolve().parent.parent / "RISC_V.sim" / "sim_1" / "behav" / "xsim"
		if not testbench_root.is_dir():
			print(f"ERROR: Cannot find testbench directory at {testbench_root}", file=sys.stderr)
			print("Please use --testbench-root /path/to/xsim/dir", file=sys.stderr)
			return 1
		args.testbench_root = str(testbench_root)
	
	# Determine tests to run
	if args.test_root:
		test_root = args.test_root.resolve()
	else:
		test_root = Path(__file__).resolve().parent

	if args.test:
		# Single test specified
		tests = [(args.test, test_root / f"{args.test}.mem")]
	elif args.task:
		# Specific task(s)
		tests = find_tests(test_root, args.task)
	else:
		# All tasks
		tests = find_tests(test_root)
	
	if not tests:
		print("No tests found!")
		return 1
	
	print(f"Found {len(tests)} test(s)")
	print(f"xsim: {xsim_path}")
	print(f"Testbench: {args.testbench_root}")
	print(f"Snapshot: {args.snapshot}\n")
	
	# Run tests
	results = {}
	for test_name, mem_path in tests:
		# Verify .mem exists
		if not mem_path.exists():
			print(f"✗ ERROR: {mem_path} not found")
			results[test_name] = False
			continue
		
		passed = run_test(test_name, args.testbench_root, args.snapshot, xsim_path, args.timeout)
		results[test_name] = passed
	
	# Summary
	print(f"\n{'='*60}")
	print("SUMMARY")
	print(f"{'='*60}")
	
	passed_count = sum(1 for p in results.values() if p)
	failed_count = len(results) - passed_count
	
	print(f"Total:  {len(results)}")
	print(f"Passed: {passed_count}")
	print(f"Failed: {failed_count}")
	
	if failed_count > 0:
		print("\nFailed tests:")
		for test, passed in results.items():
			if not passed:
				print(f"  - {test}")
	
	print(f"{'='*60}\n")
	
	return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    
    # # Run all tasks
    # python run_testbench.py

    # # Run specific task(s)
    # python run_testbench.py --task task1
    # python run_testbench.py --task task1 task2

    # # Run single test
    # python run_testbench.py --test task1/addpos

    # # Custom settings
    # python run_testbench.py --task task1 --timeout 120 --snapshot tb_datapath_behav
    
	sys.exit(main())
