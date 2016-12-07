var players = document.getElementsByClassName("Player_Name");
var str = "";
for (var i =0; i < players.length; i++)
{
	var parent = players[i].parentNode;
	var sibling = parent.children[6];
	str += players[i].innerText;
	str += ",";
	str += (sibling.innerText);
	str += "\n";
}

window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);