--TEST--
__serialize() mechanism (016): Properties are still typed after unserialization (references)
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80000) { echo "skip __serialize/__unserialize error message different in php < 8"; }
if (PHP_VERSION_ID >= 90000) { echo "skip requires php < 9.0 when testing that the deprecation has no impact on igbinary functionality\n"; }
?>
--FILE--
<?php
declare(strict_types=1);

if (PHP_VERSION_ID >= 80200) { require_once __DIR__ . '/../php82_suppress_dynamic_properties_warning.inc'; }

class Test {
    public int $i = 0;
    public ?string $s = 's';
    public object $o;
    public stdClass $stdClass;
    public array $a = [];
}
$t = new Test();
$t->i = 1;
$t->s = 'other';
$t->o = $t;
$t->std = (object)['key' => 'value'];
$t->a = [&$t->std, &$t->i, &$t->s, &$t->o];
$t->std->key = &$t->a;
var_dump($t);

var_dump(bin2hex($s = igbinary_serialize($t)));
$t2 = igbinary_unserialize($s);
var_dump($t2);
try {
    $t2->i = 'x';
} catch (Error $e) {
    echo "i: " . $e->getMessage() . "\n";
}
$t2->s = null;
try {
    $t2->s = false;
} catch (Error $e) {
    echo "s: " . $e->getMessage() . "\n";
}
$t2->s = 'other';
try {
    $t2->o = null;
} catch (Error $e) {
    echo "o: " . $e->getMessage() . "\n";
}
try {
    $t2->a = null;
} catch (Error $e) {
    echo "a: " . $e->getMessage() . "\n";
}
try {
    $t2->stdClass = $t;
} catch (Error $e) {
    echo "stdClass: " . $e->getMessage() . "\n";
}
try {
    $t2->a = $t2;
} catch (Error $e) {
    echo "a: " . $e->getMessage() . "\n";
}
var_dump($t2);
--EXPECT--
object(Test)#1 (5) {
  ["i"]=>
  &int(1)
  ["s"]=>
  &string(5) "other"
  ["o"]=>
  *RECURSION*
  ["stdClass"]=>
  uninitialized(stdClass)
  ["a"]=>
  &array(4) {
    [0]=>
    &object(stdClass)#2 (1) {
      ["key"]=>
      *RECURSION*
    }
    [1]=>
    &int(1)
    [2]=>
    &string(5) "other"
    [3]=>
    *RECURSION*
  }
  ["std"]=>
  &object(stdClass)#2 (1) {
    ["key"]=>
    &array(4) {
      [0]=>
      *RECURSION*
      [1]=>
      &int(1)
      [2]=>
      &string(5) "other"
      [3]=>
      *RECURSION*
    }
  }
}
string(176) "000000031c045465737419060e01692204010e0173220e056f746865720e016f222400010e01612219040400221c08737464436c61737319010e036b65792224030401222401040222240204032224000e03737464222404"
object(Test)#3 (5) {
  ["i"]=>
  &int(1)
  ["s"]=>
  &string(5) "other"
  ["o"]=>
  *RECURSION*
  ["stdClass"]=>
  uninitialized(stdClass)
  ["a"]=>
  &array(4) {
    [0]=>
    &object(stdClass)#4 (1) {
      ["key"]=>
      *RECURSION*
    }
    [1]=>
    &int(1)
    [2]=>
    &string(5) "other"
    [3]=>
    *RECURSION*
  }
  ["std"]=>
  &object(stdClass)#4 (1) {
    ["key"]=>
    &array(4) {
      [0]=>
      *RECURSION*
      [1]=>
      &int(1)
      [2]=>
      &string(5) "other"
      [3]=>
      *RECURSION*
    }
  }
}
i: Cannot assign string to property Test::$i of type int
s: Cannot assign bool to property Test::$s of type ?string
o: Cannot assign null to property Test::$o of type object
a: Cannot assign null to property Test::$a of type array
stdClass: Cannot assign Test to property Test::$stdClass of type stdClass
a: Cannot assign Test to property Test::$a of type array
object(Test)#3 (5) {
  ["i"]=>
  &int(1)
  ["s"]=>
  &string(5) "other"
  ["o"]=>
  *RECURSION*
  ["stdClass"]=>
  uninitialized(stdClass)
  ["a"]=>
  &array(4) {
    [0]=>
    &object(stdClass)#4 (1) {
      ["key"]=>
      *RECURSION*
    }
    [1]=>
    &int(1)
    [2]=>
    &string(5) "other"
    [3]=>
    *RECURSION*
  }
  ["std"]=>
  &object(stdClass)#4 (1) {
    ["key"]=>
    &array(4) {
      [0]=>
      *RECURSION*
      [1]=>
      &int(1)
      [2]=>
      &string(5) "other"
      [3]=>
      *RECURSION*
    }
  }
}