--TEST--
__wakeup can modify properties without affecting other objects
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php

class Obj {
	private static $count = 1;

	public $a;

	function __construct($a) {
		$this->a = $a;
	}

	public function __wakeup() {
		echo "call wakeup\n";
		$this->a[] = "end";
	}
}

function main() {
	$array = ["test"];  // array (not a reference, but should be copied on write)
	$a = new Obj($array);
	$b = new Obj($array);
	$variable = [$a, $b];
	$serialized = igbinary_serialize($variable);
	printf("%s\n", bin2hex($serialized));
	$unserialized = igbinary_unserialize($serialized);
	var_dump($unserialized);
}
main();
--EXPECTF--
00000003190204001c034f626a19010e0161190104000e047465737404011500190112012402
call wakeup
call wakeup
array(2) {
  [0]=>
  object(Obj)#%d (1) {
    ["a"]=>
    array(2) {
      [0]=>
      string(4) "test"
      [1]=>
      string(3) "end"
    }
  }
  [1]=>
  object(Obj)#%d (1) {
    ["a"]=>
    array(2) {
      [0]=>
      string(4) "test"
      [1]=>
      string(3) "end"
    }
  }
}
