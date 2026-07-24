{
    print

    if (index($0, ".global ") != 0) {
        symbol = $0
        sub(/^.*\.global[[:space:]]+/, "", symbol)
        sub(/[[:space:];@].*$/, "", symbol)
        print "\t.hidden " symbol
    }
}
