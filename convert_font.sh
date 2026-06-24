awk '
/^\/\*/ {
    if (NR > 1)
        print "\t},"
    print $0 " {"
    count=0
    next
}
{
    print
}
END {
    print "\t},"
}
' font.c > font_new.c