--TEST--
Test unserializing valid enums
--SKIPIF--
<?php if (PHP_VERSION_ID < 80100) { echo "skip enums requires php 8.1"; } ?>
--FILE--
<?php

enum Suit {
    case Hearts;
    case Diamonds;
    case Spades;
    case Clubs;
}
$arr = ['Hearts' => Suit::Hearts];
$arr[1] = &$arr['Hearts'];
$serArray = igbinary_serialize($arr);
echo urlencode($serArray), "\n";
$result = igbinary_unserialize($serArray);
var_dump($result);
$result[1] = 'new';
var_dump($result);

?>
--EXPECT--
%00%00%00%02%14%02%11%06Hearts%25%17%04Suit%27%0E%00%06%01%25%22%01
array(2) {
  ["Hearts"]=>
  enum(Suit::Hearts)
  [1]=>
  enum(Suit::Hearts)
}
array(2) {
  ["Hearts"]=>
  &string(3) "new"
  [1]=>
  &string(3) "new"
}