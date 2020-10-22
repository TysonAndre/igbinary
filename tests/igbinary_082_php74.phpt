--TEST--
igbinary object with typed properties with reference to ArrayObject
--SKIPIF--
<?php if (PHP_VERSION_ID < 70400) die("skip test requires typed properties"); ?>
--FILE--
<?php
class TestClass {
  private ArrayAccess $env;
  public function setEnv(ArrayObject &$e) {
    $this->env = &$e;
  }
}

$arrayObject = new ArrayObject();

$testClass = new TestClass();
$testClass->setEnv($arrayObject);

var_dump(igbinary_unserialize(igbinary_serialize($testClass)));
?>
--EXPECTF--
object(TestClass)#%d (1) {
  ["env":"TestClass":private]=>
  object(ArrayObject)#%d (1) {
    ["storage":"ArrayObject":private]=>
    array(0) {
    }
  }
}
