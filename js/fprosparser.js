var players = document.getElementsByClassName("player-name");
var str = "";
for (var i =0; i < players.length; i++)
{
	var name = players[i].textContent;
	str += name;
	var row = players[i].parentNode.parentNode;
	var children = row.children;
	var allZero = true;
	for (var j = 1; j < children.length; j++)
	{
		str += ",";
		str += children[j].textContent;
		if (parseInt(children[j].textContent) != 0)
		{
			allZero = false;
		}
	}
	str += "\n";
	if (allZero)
	{
		break;
	}
}

window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);