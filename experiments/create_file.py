import sys

def parse_size(size_str):
    units = {
        'Byt': 1,
        'KiB': 1024,
        'MiB': 1024 * 1024
    }
    try:
        num, unit = size_str[:-3], size_str[-3:]
        return int(num) * units[unit]
    except ValueError:
        raise ValueError("Invalid size format")

def create_file(size_bytes, filename):
    with open(filename, 'w') as f:
        f.write('a' * size_bytes)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <filesize>")
        sys.exit(1)

    try:
        size_str = sys.argv[1]
        size_bytes = parse_size(size_str)
        filename = f'file_{size_str}.txt'
    except ValueError as e:
        print(e)
        sys.exit(1)

    create_file(size_bytes, filename)
    print(f"File {filename} with {size_bytes} bytes created successfully.")

