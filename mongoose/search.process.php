#!/usr/bin/php-cgi
<html><body><center><p><br><p><br><font face=arial>

<font size=7 color=darkblue>ZoekMuis</font>
<p>
<form enctype="multipart/form-data" action="./search.process.php" method="post">
<input name="myquery" size="60">
<input name="Search" value="search" type="submit"> </form>
<p><br><p></center>
<?php

echo '<hr><p><font size=6 color=darkblue>Results</font><p>';

// $myquery contains the query from the search engine
$myquery = $_POST['myquery']; 

// Write the query terms to a local file to minimize security problems
$handle  = fopen("../zoekmuis/queryterms.txt","w");
fwrite($handle,$myquery);
fclose($handle);

$commandstring = '(cd ../zoekmuis && ./webquery)';

//Using backticks one way for PHP to call an external program and return the output
$myoutput =`$commandstring`;

echo '<p>(1) ';
echo $myoutput.'<p>';
?>

</font></body></html>
