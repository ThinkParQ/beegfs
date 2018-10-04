BEGIN {
	indent = "";
}

$1 == "//-" {
	sub("^   ", "", indent);
}

$1 ~ "^//[+-/0]$", $1 ~ "^//[+-/0]$" {
	line = $0;
	sub("^[ \t]*//. ?", "", line);

	if (line ~ /↩$/)
		sub("↩$", "\\", line);

	if ($1 == "//0")
		print line;
	else
		print indent line;
}

$1 == "//+" {
	indent = indent "   ";
}

$1 !~ "^//[+-/0]$" {
	print "";
}
