#!/bin/bash
# generate_build_mks.sh

for dir in shell fs boot arch/x86 drivers lib mm kernel screen tests logo ui hw; do
    mk="$dir/build.mk"
    echo "# Auto-generated build.mk for $dir" > "$mk"
    
    # C files
    for f in "$dir"/*.c; do
        [ -e "$f" ] || continue
        base=$(basename "$f" .c)
        echo "obj-y += $dir/$base.o" >> "$mk"
    done
    
    # Assembly files
    for f in "$dir"/*.S; do
        [ -e "$f" ] || continue
        base=$(basename "$f" .S)
        echo "obj-y += $dir/$base.o" >> "$mk"
    done
    
    echo "Generated $mk"
done