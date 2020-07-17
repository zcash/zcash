echo "{\"files\":{$(
find . -type f |  	# Get list of file paths
grep -v $1 |		# Exclude Makefile hashes
grep -v '[.]stamp_' |	# Exclude Makefile stamps
sed 's|^[.]/||' |	# Remove leading ./
sort |			# Sort (for uniformity)
xargs $2 |		# Get SHA256 hashes (assumes standard 'H(A) A' format)
awk -v OFS='":"' '{print $2, $1}' |	#   'H(A) A'  ->  'A":"H(A)'
sed 's|^|"|' |				#  'A":"H(A)' -> '"A":"H(A)'
sed 's|$|"|' |				# '"A":"H(A)' -> '"A":"H(A)"'
tr '\n' ',' |		# Concatenate lines with commas
sed 's|,$||'		# Remove any trailing comma (to fit JSON spec)
)},\"package\":$3}" > .cargo-checksum.json
