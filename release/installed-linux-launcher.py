#!/usr/bin/env python3
"""Start the packaged launcher with a host path visible to Proton."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main(arguments):
    package = Path(__file__).resolve().parent
    command = [sys.executable, str(package / "ed-up-launcher-linux.py"), "--desktop"]
    wrapper_args = arguments[:arguments.index("--")] if "--" in arguments else arguments
    if "--dry-run" in wrapper_args:
        return subprocess.call(command + arguments)

    # Steam Linux Runtime can replace system directories. Put the Windows EXE
    # in a private host directory that the wrapper can expose to Proton.
    desktop_cache = Path(os.environ.get("XDG_CACHE_HOME", ""))
    if not desktop_cache.is_absolute():
        desktop_cache = Path.home() / ".cache"
    cache = desktop_cache / "ed-up-launcher"
    cache.mkdir(mode=0o700, parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="launch-", dir=cache) as folder:
        exe = Path(folder) / "ed-up-launcher.exe"
        shutil.copy2(package / exe.name, exe)
        return subprocess.call(command + ["--launcher-exe", str(exe)] + arguments)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
