--TEST--
igbinary and edge cases unserializing array keys
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
set_error_handler(function ($errno, $errstr) {
    echo "$errstr\n";
});
var_dump(bin2hex($s = igbinary_serialize(['key' => true])));
// 3-byte string truncated in the middle of the array key
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x19\x01\x0e\x03\x6b\x65\x79"));

// null instead of a string - skip over the entry
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x19\x01\x0e\x03\x6b\x65\x79"));
?>
--EXPECT--
string(24) "0000000319010e036b657903"
igbinary_unserialize_chararray: end-of-data
NULL
array(0) {
}
