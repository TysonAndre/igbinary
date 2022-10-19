--TEST--
Correctly unserialize multiple references in arrays
--INI--
igbinary.use_v3_serialize_format=1
igbinary.compact_strings = On
--FILE--
<?php
class Foo{}
$a = array("A");
$a[1] = &$a[0];
$a[2] = &$a[1];
$a[3] = &$a[2];
$a[4] = false;
$a[5] = &$a[4];
$a[6] = new Foo();
$a[7] = &$a[6];
$a[8] = &$a[7];
$a[9] = array(33);
$a[10] = new stdClass();
$a[10]->prop = &$a[8];
$a[11] = &$a[10];
$a[12] = $a[9];
$ig_ser = igbinary_serialize($a);
printf("ig_ser=%s\n", bin2hex($ig_ser));
$ig = igbinary_unserialize($ig_ser);
var_dump($ig);
$f = &$ig[3];
$f = 'V';
$g = &$ig[5];
$g = 'H';
$h = $ig[10];
$h->prop = 'S';
var_dump($ig);
--EXPECTF--
ig_ser=00000003190d0400220e01410401222401040222240104032224010404220204052224020406221c03466f6f1900040722240304082224030409190104000421040a221c08737464436c61737319010e0470726f70222403040b222405040c2404
array(13) {
  [0]=>
  &string(1) "A"
  [1]=>
  &string(1) "A"
  [2]=>
  &string(1) "A"
  [3]=>
  &string(1) "A"
  [4]=>
  &bool(false)
  [5]=>
  &bool(false)
  [6]=>
  &object(Foo)#%d (0) {
  }
  [7]=>
  &object(Foo)#%d (0) {
  }
  [8]=>
  &object(Foo)#%d (0) {
  }
  [9]=>
  array(1) {
    [0]=>
    int(33)
  }
  [10]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &object(Foo)#%d (0) {
    }
  }
  [11]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &object(Foo)#%d (0) {
    }
  }
  [12]=>
  array(1) {
    [0]=>
    int(33)
  }
}
array(13) {
  [0]=>
  &string(1) "V"
  [1]=>
  &string(1) "V"
  [2]=>
  &string(1) "V"
  [3]=>
  &string(1) "V"
  [4]=>
  &string(1) "H"
  [5]=>
  &string(1) "H"
  [6]=>
  &string(1) "S"
  [7]=>
  &string(1) "S"
  [8]=>
  &string(1) "S"
  [9]=>
  array(1) {
    [0]=>
    int(33)
  }
  [10]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &string(1) "S"
  }
  [11]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &string(1) "S"
  }
  [12]=>
  array(1) {
    [0]=>
    int(33)
  }
}
