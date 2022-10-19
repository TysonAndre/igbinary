--TEST--
igbinary and not enough data for array
--INI--
igbinary.use_v3_serialize_format=1
display_errors=stderr
error_reporting=E_ALL
--FILE--
<?php
echo "One byte\n";
igbinary_unserialize("\x00\x00\x00\x03\x19");
echo "Two byte\n";
igbinary_unserialize("\x00\x00\x00\x03\x1a\x01");
igbinary_unserialize("\x00\x00\x00\x03\x1a");
igbinary_unserialize("\x00\x00\x00\x03\x1a\x00\x01");
igbinary_unserialize("\x00\x00\x00\x03\x1a\x00\x01\x00"); // '00' is invalid type for array element
var_dump(igbinary_unserialize("\x00\x00\x00\x03\x1a\x00\x00"));  // Permitted
echo "Four byte\n";
igbinary_unserialize("\x00\x00\x00\x03\x1b\x00");
igbinary_unserialize("\x00\x00\x00\x03\x1b\x00\x00\x01");
igbinary_unserialize("\x00\x00\x00\x03\x1b\x00\x00\x00\x01");
?>
--EXPECTF--
One byte
Warning: igbinary_unserialize_v3_array: end-of-data in %sigbinary_074.php on line 3
Two byte
Warning: igbinary_unserialize_v3_array: end-of-data in %sigbinary_074.php on line 5
Warning: igbinary_unserialize_v3_array: end-of-data in %sigbinary_074.php on line 6
Warning: igbinary_unserialize_v3_array: data size 0 smaller that requested array length 1. in %sigbinary_074.php on line 7
Warning: igbinary_unserialize_v3_array: unknown key type '00', position 8 in %sigbinary_074.php on line 8
array(0) {
}
Four byte
Warning: igbinary_unserialize_v3_array: end-of-data in %sigbinary_074.php on line 11
Warning: igbinary_unserialize_v3_array: end-of-data in %sigbinary_074.php on line 12
Warning: igbinary_unserialize_v3_array: data size 0 smaller that requested array length 1. in %sigbinary_074.php on line 13
