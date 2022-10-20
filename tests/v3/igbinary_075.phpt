--TEST--
igbinary and not enough data for array of object properties
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
set_error_handler(function ($errno, $errstr) {
    echo "$errstr\n";
});
class X {}
var_dump(bin2hex($s = igbinary_serialize(new X())));
echo "One byte\n";
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x1f\x01\x58\x19"));
echo "Two byte\n";
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x1f\x01\x58\x1a"));
igbinary_unserialize("\x00\x00\x00\x03\x1f\x01\x58\x1a\xff");
echo "Four byte\n";
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x1f\x01\x58\x1b"));
igbinary_unserialize("\x00\x00\x00\x03\x1f\x01\x58\x1b\x00\x00\x01");
?>
--EXPECTF--
string(18) "000000031f015818"
One byte
igbinary_unserialize_v3_object_properties: end-of-data
NULL
Two byte
igbinary_unserialize_v3_object_properties: end-of-data
NULL
igbinary_unserialize_v3_object_properties: end-of-data
Four byte
igbinary_unserialize_v3_object_properties: end-of-data
NULL
igbinary_unserialize_v3_object_properties: end-of-data