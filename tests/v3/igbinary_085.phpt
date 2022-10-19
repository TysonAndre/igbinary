--TEST--
Properly free unexpected duplicate fields when unserializing arrays
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
// Duplicate fields wouldn't be created by calls to igbinary_serialize,
// but make sure to handle them when unserializing corrupt data.
$s = new stdClass();
$ser = igbinary_serialize(['xx' => $s, 'yy' => $s]);
echo urlencode($ser), "\n";
$result = igbinary_unserialize(str_replace('xx', 'yy', $ser));
var_dump($result);
$ser = igbinary_serialize([0x66 => $s, 0x77 => $s]);
echo urlencode($ser), "\n";
$result2 = igbinary_unserialize(str_replace("\x66", "\x77", $ser));
var_dump($result2);
?>
--EXPECT--
%00%00%00%03%19%02%0E%02xx%1C%08stdClass%19%00%0E%02yy%24%01
array(1) {
  ["yy"]=>
  object(stdClass)#2 (0) {
  }
}
%00%00%00%03%19%02%04f%1C%08stdClass%19%00%04w%24%01
array(1) {
  [119]=>
  object(stdClass)#3 (0) {
  }
}