
<?php 
function translate($filterstring, $prefix="", $comment="") {
	$reg_float = "[-]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?";

	$b0 = 0;
	$b1 = 0;
	$invgain = 0;
	
	$lines = explode("\n", $filterstring);

	foreach ($lines as $line) {
		if (preg_match("/gain at centre/", $line)) {
			preg_match("/mag = (". $reg_float .")/", $line, $matches);
			$f_invgain = 1.0 / ((float) $matches[1]);
		}
		if (preg_match("/y\[n\-\ 2\]/", $line)) {
			preg_match("/(". $reg_float .")\ \*/", $line, $matches);
			$f_b0 = (float) $matches[1];
		}
		if (preg_match("/y\[n\-\ 1\]/", $line)) {
			preg_match("/(". $reg_float .")\ \*/", $line, $matches);
			$f_b1 = (float) $matches[1];
		}

	}
		$invgain = (int) ($f_invgain * (4096));
		$b0 = (int) ($f_b0 * (4096));
		$b1 = (int) ($f_b1 * (4096));

		echo "// ".$prefix.": ".$comment."\n";
		echo "#define ".$prefix."INVGAIN ".$invgain."\n";
		echo "#define ".$prefix."B0 ".$b0."\n";
		echo "#define ".$prefix."B1 ".$b1."\n";
}

echo "//These were created using Interactive Digital Filter Design, http://www-users.cs.york.ac.uk/~fisher/mkfilter \n";
echo "// (and made fixed point with filters.php)\n";
echo "// they are shifted to have 12 fractional bits (ie. Q20.12 format)\n";

translate("
gain at centre:   mag = 1.059375143e+01   phase =  -0.2500000000 pi
     + (  0.8665049325 * y[n- 1])
", "lowband_", "160-320 HZ"
);


translate("
gain at centre:   mag = 2.553755626e+00   phase =   0.0134301505 pi
     + ( -0.2175366846 * y[n- 2])
     + ( -0.2059706493 * y[n- 1])
", "midband_", "2500-4000 HZ"
);

translate("
gain at centre:   mag = 2.605495504e+00   phase =   0.1104971338 pi
    + ( -0.2781790780 * y[n- 2])
     + ( -1.1875661839 * y[n- 1])
", "highband_", "4000-5500 HZ"
);

?>
