--TEST--
__wakeup can replace a copy of the object referring to the root node.
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php

#[AllowDynamicProperties]
class Obj {
	function __construct($a) {
		$this->a = $a;
	}

	public function __wakeup() {
		echo "Calling __wakeup\n";
		$this->a = "replaced";
	}
}

$a = new stdClass();
$a->obj = new Obj($a);;
$serialized = igbinary_serialize($a);
printf("%s\n", bin2hex($serialized));
$unserialized = igbinary_unserialize($serialized);
var_dump($unserialized);
--EXPECTF--
000000031c08737464436c61737319010e036f626a1c034f626a19010e01612400
Calling __wakeup
object(stdClass)#%d (1) {
  ["obj"]=>
  object(Obj)#%d (1) {
    ["a"]=>
    string(8) "replaced"
  }
}
