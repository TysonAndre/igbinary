--TEST--
Check for handling of IS_INDIRECT in arrays
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
$globalVar = 123;
$otherGlobalVar = &$globalVar;

call_user_func(function () {
    $x = $GLOBALS;
    foreach ($x as $key => $value) {
        if (!in_array($key, ['globalVar', 'otherGlobalVar'])) {
            unset($x[$key]);
        }
    }
    var_dump($x);
    $ser = igbinary_serialize($x);
    echo urlencode($ser) . "\n";
    var_dump(igbinary_unserialize($ser));

});
--EXPECT--
array(2) {
  ["globalVar"]=>
  &int(123)
  ["otherGlobalVar"]=>
  &int(123)
}
%00%00%00%03%19%02%0E%09globalVar%22%04%7B%0E%0EotherGlobalVar%22%24%01
array(2) {
  ["globalVar"]=>
  &int(123)
  ["otherGlobalVar"]=>
  &int(123)
}