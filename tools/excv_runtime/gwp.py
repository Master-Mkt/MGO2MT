# Generated converter-only source; historical analysis entry points omitted.
import hashlib
from pathlib import Path

def record(path):
    path = Path(path).resolve()
    return {'path': str(path), 'size': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
