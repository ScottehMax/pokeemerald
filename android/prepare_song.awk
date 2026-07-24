# Song commands and headers retain the GBA's four-byte operands. Android
# encodes pointer operands relative to their own address so the shared object
# remains position independent without changing the asset layout.
BEGIN { in_header = 0; aligned = 0 }
$0 ~ "^[[:space:]]*" song_name ":[[:space:]]*$" {
    print "\tasset_ptr_align"
    in_header = 1
}
!in_header && $0 ~ /^[[:space:]]*\.word[[:space:]]+/ {
    sub(/\.word/, "script_ptr")
}
in_header && $0 ~ /^[[:space:]]*\.word[[:space:]]+/ {
    if (!aligned) {
        print "\tasset_ptr_align"
        aligned = 1
    }
    sub(/\.word/, "asset_ptr")
}
{ print }
