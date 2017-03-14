#!/usr/bin/php-cgi
<?php
require( 'header.php' );
?>

<div class="container-background">
<div class="container">
<div class="result-list">

<?php
// $myquery contains the query from the search engine
$myquery = $_POST['myquery']; 

// $type contains either 'web' or 'images'
$type =$_POST['type'];

// Write the query terms to a local file to minimize security problems
$handle  = fopen("../zoekmuis/queryterms.txt","w");
fwrite($handle,$myquery);
fclose($handle);

$commandstring = "(cd ../zoekmuis && ./webquery --$type)";

//Using backticks one way for PHP to call an external program and return the output
$myoutput =`$commandstring`;

echo $myoutput;
?>

</div></div></div>
</body></html>
