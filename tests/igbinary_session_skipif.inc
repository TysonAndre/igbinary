<?php
if (!extension_loaded('session')) {
	exit('skip session extension not loaded');
}
ob_start();
phpinfo(INFO_MODULES);
$skipif_str = ob_get_clean();

$skipif_array = explode("\n", $skipif_str);
$skipif_array = preg_grep('/^igbinary session support.*yes/', $skipif_array);
if (!$skipif_array) {
	exit('skip igbinary session handler not available');
}
