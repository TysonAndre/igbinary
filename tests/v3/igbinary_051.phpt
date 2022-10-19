--TEST--
Object test, __wakeup (With multiple references)
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

class Obj {
	var $a;
	var $b;

	function __construct($a, $b) {
		$this->a = $a;
		$this->b = $b;
	}

	function __wakeup() {
		$this->b = $this->a * 3;
	}
}

function main() {
	$o = new Obj(1, 2);
	$variable = array(&$o, &$o);
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);

	echo substr(bin2hex($serialized), 8), "\n";
	echo $unserialized[0]->b === 3 && $unserialized[0]->a === 1 ? 'OK' : 'ERROR';
	echo "\n";
	$unserialized[0] = 'a';
	var_dump($unserialized);
}

main();
--EXPECT--
19020400221c034f626a19020e016104010e016204020401222401
OK
array(2) {
  [0]=>
  &string(1) "a"
  [1]=>
  &string(1) "a"
}
