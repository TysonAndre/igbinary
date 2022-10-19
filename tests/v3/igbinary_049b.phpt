--TEST--
Correctly unserialize multiple references in objects
--INI--
igbinary.compact_strings = On
igbinary.use_v3_serialize_format=1
--FILE--
<?php
class Foo{}
$a = new stdClass();
$a->x0 = NULL;
$a->x1 = &$a->x0;
$a->x2 = &$a->x1;
$a->x3 = &$a->x2;
$a->x4 = false;
$a->x5 = &$a->x4;
$a->x6 = new Foo();
$a->x7 = &$a->x6;
$a->x8 = &$a->x7;
$a->x9 = array(33);
$a->x10 = new stdClass();
$a->x10->prop = &$a->x8;
$a->x11 = &$a->x10;
$a->x12 = $a->x9;
$ig_ser = igbinary_serialize($a);
printf("ig_ser=%s\n", bin2hex($ig_ser));
$ig = igbinary_unserialize($ig_ser);
$f = &$ig->x3;
$f = 'V';
$g = &$ig->x5;
$g = 'H';
$h = $ig->x10;
$h->prop = 'S';
var_dump($ig);
--EXPECTF--
ig_ser=000000031c08737464436c617373190d0e02783022010e0278312224010e0278322224010e0278332224010e02783422020e0278352224020e027836221c03466f6f19000e0278372224030e0278382224030e0278391901040004210e0378313022150019010e0470726f702224030e037831312224050e037831322404
object(stdClass)#%d (13) {
  ["x0"]=>
  &string(1) "V"
  ["x1"]=>
  &string(1) "V"
  ["x2"]=>
  &string(1) "V"
  ["x3"]=>
  &string(1) "V"
  ["x4"]=>
  &string(1) "H"
  ["x5"]=>
  &string(1) "H"
  ["x6"]=>
  &string(1) "S"
  ["x7"]=>
  &string(1) "S"
  ["x8"]=>
  &string(1) "S"
  ["x9"]=>
  array(1) {
    [0]=>
    int(33)
  }
  ["x10"]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &string(1) "S"
  }
  ["x11"]=>
  &object(stdClass)#%d (1) {
    ["prop"]=>
    &string(1) "S"
  }
  ["x12"]=>
  array(1) {
    [0]=>
    int(33)
  }
}
