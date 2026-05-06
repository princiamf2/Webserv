<?php
header("Content-Type: text/plain\n\n");
echo "PHP OK\n";
echo "METHOD=" . $_SERVER['REQUEST_METHOD'] . "\n";
echo "QUERY=" . $_SERVER['QUERY_STRING'] . "\n";
?>
