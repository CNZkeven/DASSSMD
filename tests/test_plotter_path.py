from pathlib import Path
import subprocess


def test_plotter_path_host(tmp_path):
    root = Path(__file__).resolve().parents[1]
    executable = tmp_path / "plotter_path_host"
    compile_result = subprocess.run(
        [
            "cc",
            "-std=c99",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(root / "User/plotter"),
            str(root / "User/plotter/plotter_path.c"),
            str(root / "tests/plotter_path_host.c"),
            "-o",
            str(executable),
        ],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )
    assert compile_result.returncode == 0, (
        f"host compile failed:\n{compile_result.stdout}{compile_result.stderr}"
    )

    run_result = subprocess.run(
        [str(executable)],
        cwd=root,
        capture_output=True,
        text=True,
        check=False,
    )
    assert run_result.returncode == 0, (
        f"host test failed:\n{run_result.stdout}{run_result.stderr}"
    )
    assert run_result.stdout.strip() == "plotter path host tests passed"
