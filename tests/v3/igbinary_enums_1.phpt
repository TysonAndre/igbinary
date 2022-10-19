--TEST--
Test unserializing valid enums
--INI--
igbinary.use_v3_serialize_format=1
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
$ser = igbinary_serialize(Suit::Hearts);
echo urlencode($ser), "\n";
var_dump(igbinary_unserialize($ser));
var_dump(igbinary_unserialize($ser));
$serArray = igbinary_serialize([Suit::Hearts, Suit::Diamonds, Suit::Spades, Suit::Clubs, Suit::Clubs, 'Diamonds' => 'Diamonds']);
echo urlencode($serArray), "\n";
var_dump(igbinary_unserialize($serArray));

?>
--EXPECT--
%00%00%00%03%1C%04Suit%23%0E%06Hearts
enum(Suit::Hearts)
enum(Suit::Hearts)
%00%00%00%03%19%06%04%00%1C%04Suit%23%0E%06Hearts%04%01%15%00%23%0E%08Diamonds%04%02%15%00%23%0E%06Spades%04%03%15%00%23%0E%05Clubs%04%04%24%04%12%02%12%02
array(6) {
  [0]=>
  enum(Suit::Hearts)
  [1]=>
  enum(Suit::Diamonds)
  [2]=>
  enum(Suit::Spades)
  [3]=>
  enum(Suit::Clubs)
  [4]=>
  enum(Suit::Clubs)
  ["Diamonds"]=>
  string(8) "Diamonds"
}