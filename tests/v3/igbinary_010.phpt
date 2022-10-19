--TEST--
Array test
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable, $test) {
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);

	echo $type, "\n";
	echo substr(bin2hex($serialized), 8), "\n";
	echo $test || $unserialized == $variable ? 'OK' : 'ERROR';
	echo "\n";
}

$a = array(
	'a' => array(
		'b' => 'c',
		'd' => 'e'
	),
	'f' => array(
		'g' => 'h'
	)
);

test('array', $a, false);

/*
 * you can add regression tests for your extension here
 *
 * the output of your test code has to be equal to the
 * text in the --EXPECT-- section below for the tests
 * to pass, differences between the output and the
 * expected text are interpreted as failure
 *
 * see TESTING.md for further information on
 * writing regression tests
 */
?>
--EXPECT--
array
19020e016119020e01620e01630e01640e01650e016619010e01670e0168
OK
