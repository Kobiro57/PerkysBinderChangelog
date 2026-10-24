import subprocess
import traceback

try:
    result = subprocess.run(
        ["build\\KeybinderTests.exe"], capture_output=True, text=True
    )
    print("STDOUT:")
    print(result.stdout)
    print("STDERR:")
    print(result.stderr)
    print(f"Return Code: {result.returncode}")
except Exception as e:
    print(traceback.format_exc())
