--TEST--
__wakeup can add dynamic properties without affecting other objects
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php

#[AllowDynamicProperties]
class Obj {
	// Testing $this->a being a dynamic property.

	function __construct($a) {
		$this->a = $a;
	}

	public function __wakeup() {
		echo "Calling __wakeup\n";
		for ($i = 0; $i < 10000; $i++) {
			$this->{'b' . $i} = 42;
		}
	}
}

function main() {
	$array = array("roh");  // array (not a reference, but should be copied on write)
	$a = new Obj($array);
	$b = new Obj($array);
	$c = new Obj(null);
	$variable = array($a, $b, $c);
	$serialized = igbinary_serialize($variable);
	printf("%s\n", bin2hex($serialized));
	$unserialized = igbinary_unserialize($serialized);
	echo "Called igbinary_unserialize\n";
	for ($a = 0; $a < 3; $a++) {
		for ($i = 0; $i < 10000; $i++) {
			if ($unserialized[$a]->{'b' . $i} !== 42) {
				echo "Fail $a b$i\n";
				return;
			}
			unset($unserialized[$a]->{'b' . $i});
		}
	}
	var_dump($unserialized);
}
main();
--EXPECTF--
00000003190304001c034f626a19010e0161190104000e03726f6804011500190112012402040215001901120101
Calling __wakeup
Calling __wakeup
Calling __wakeup
Called igbinary_unserialize
array(3) {
  [0]=>
  object(Obj)#%d (1) {
    ["a"]=>
    array(1) {
      [0]=>
      string(3) "roh"
    }
  }
  [1]=>
  object(Obj)#%d (1) {
    ["a"]=>
    array(1) {
      [0]=>
      string(3) "roh"
    }
  }
  [2]=>
  object(Obj)#%d (1) {
    ["a"]=>
    NULL
  }
}
