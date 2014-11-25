<?php

//Test the filter function by generating a series of nominal correct values for a known input series


$y0=0.0;
$y1=0.0;
$y2=0.0;
$x0=0.0;
$x1=0.0;
$x2=0.0;

$samples = array(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
$results = array();


function step($sample, $b0, $b1, $gain) {
	 global $x0, $x1, $x2, $y0, $y1, $y2;

	 $x0 = $x1; $x1 = $x2;
	 $x2 = $sample/$gain;

	 $y0 = $y1; $y1 = $y2;
	 $y2 = ($x2-$x0) + ($b0 * $y0) + ($b1 * $y1);

	 return $y2;
}

$gain = (float) 6.338937938e+00;
$b0 = (float) -0.6879820274;
$b1 = (float) 1.4649022688;


echo sprintf("\ninvgain: %04d,  b0: %04d, b1: %04d\n\n", (int)(1/$gain*1024), (int) ($b0*1024), (int) ($b1*1024));

step(1.0, $b0, $b1, $gain);
//for ($i=0; $i<1000; $i++)
//    step(0, $b0, $b1, $gain);

for ($i=0; $i<count($samples); $i++) {
    $results[] = step(0, $b0, $b1, $gain);
    //$results[] = step($samples[$i], $b0, $b1, $gain);
    echo sprintf("%04d => x0: %04d, x1: %04d, x2: %04d, y0: %04d, y1: %04d, y2: %04d\n",
    	 (int) ($samples[$i] * 2048),
    	 (int) ($x0 * 2048),
    	 (int) ($x1 * 2048),
    	 (int) ($x2 * 2048),
    	 (int) ($y0 * 2048),
    	 (int) ($y1 * 2048),
    	 (int) ($y2 * 2048)
    );
/*
    echo sprintf("%01.4f => %01.4f    -    %04d => %04d    -    %04x => %04x\n",
    	 $samples[$i],
    	 $results[$i],
    	 (int) ($samples[$i]*2048),
    	 (int)($results[$i]*2048),
    	 (int) ($samples[$i]*2048),
    	 (int)($results[$i]*2048)
    );
*/
}


