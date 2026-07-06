#!/bin/bash
# tools/vendor_acpica.sh
set -e
curl -L https://github.com/acpica/acpica/archive/refs/heads/master.tar.gz -o /tmp/acpica.tar.gz
rm -rf /tmp/acpica-src && mkdir /tmp/acpica-src
tar xzf /tmp/acpica.tar.gz -C /tmp/acpica-src --strip-components=1
mkdir -p third_party/acpica
cp -r /tmp/acpica-src/source/components third_party/acpica/
cp -r /tmp/acpica-src/source/include third_party/acpica/
rm -rf third_party/acpica/components/debugger
rm -rf third_party/acpica/components/disassembler
rm -rf /tmp/acpica-src /tmp/acpica.tar.gz
echo "[x] Vendored ACPICA"