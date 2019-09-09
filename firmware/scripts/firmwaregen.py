#!/usr/bin/env python3
"""
Places information about firmware into the passed file
"""

import argparse, subprocess, textwrap, os

def write_if_different(content, filename):
    if os.path.exists(filename):
        with open(filename) as f:
            old_content = f.read()
    else:
        old_content = ''
    if old_content != content:
        with open(filename, 'w') as f:
            f.write(content)
def main():
    parser = argparse.ArgumentParser(description='Places repository information into the passed C file',
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=textwrap.dedent(__doc__))
    parser.add_argument('file', help='File to update, if needed')
    args = parser.parse_args()

    version = subprocess.check_output(['git', 'describe', '--dirty']).strip()
    contents = 'const char *firmware_version = "{}";\n'.format(version.decode('utf8'));
    write_if_different(contents, args.file)

if __name__ == '__main__':
    main()

