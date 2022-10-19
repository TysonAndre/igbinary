--TEST--
Check for reference serialisation
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if(!extension_loaded('igbinary')) {
	echo "skip no igbinary";
}
--FILE--
<?php

function test($type, $variable, $normalize = false) {
	// Canonicalize $variable
	if ($normalize) {
		$variable = unserialize(serialize($variable));
	}
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);


	$serialize_act = serialize($unserialized);
	$serialize_exp = serialize($variable);

	echo $type, "\n";
	echo substr(bin2hex($serialized), 8), "\n";
	echo $serialize_act === $serialize_exp ? 'OK' : 'ERROR', "\n";

	ob_start();
	var_dump($variable);
	$dump_exp = ob_get_clean();
	ob_start();
	var_dump($unserialized);
	$dump_act = ob_get_clean();

	if ($dump_act !== $dump_exp) {
		echo "But var dump differs:\nActual:\n", $dump_act, "\nExpected\n", $dump_exp, "\n";
		if ($normalize) {
			echo "(Was normalized)\n";
		}
	}

	if ($serialize_act !== $serialize_exp) {
		echo "But serialize differs:\nActual:\n", $serialize_act, "\nExpected:\n", $serialize_exp, "\n";
	}
}

$a = array('foo');

test('array($a, $a)', array($a, $a));
test('array(&$a, &$a)', array(&$a, &$a));

$a = array(null);
$b = array(&$a);
$a[0] = &$b;

test('cyclic $a = array(&array(&$a)) - normalized', $a, true);
--EXPECT--
array($a, $a)
19020400190104000e03666f6f04012401
OK
array(&$a, &$a)
1902040022190104000e03666f6f0401222401
OK
cyclic $a = array(&array(&$a)) - normalized
19010400221901040019010400222401
OK
