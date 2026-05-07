<?php
header("Content-Type: text/plain");

echo "PHP CGI POST reader\n";
echo "METHOD=" . (isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : '') . "\n";
echo "QUERY=" . (isset($_SERVER['QUERY_STRING']) ? $_SERVER['QUERY_STRING'] : '') . "\n";
echo "CONTENT_TYPE=" . (isset($_SERVER['CONTENT_TYPE']) ? $_SERVER['CONTENT_TYPE'] : '') . "\n";

$rawBody = file_get_contents('php://input');
echo $rawBody . "\n";


if (!empty($_POST)) {
    foreach ($_POST as $key => $value) {
        echo $key . "=" . $value . "\n";
    }
} else {
    echo "(empty)\n";
}
?>