var str = "";
getData();
function delayGetData()
{
	setTimeout(getData, 2000);
}
function getData()
{
	var pos = "";
	var table = document.getElementsByClassName("players")[0].getElementsByTagName("tbody")[0];
	var players = table.getElementsByTagName("tr");
	for (var i =0; i < players.length; i++)
	{
		var row = players[i];
		var cols = row.getElementsByTagName("td");
		var name = cols[1].getElementsByClassName("ysf-player-name")[0].innerText;
		var posarr = name.split("-");
		var pos = posarr[posarr.length - 1];
		name = name.split("-")[0].trim();
		if (name == "New York NYJ")
		{
			name = "New York Jets";
		}
		else if (name == "New York NYG")
		{
			name = "New York Giants";
		}
		else
		{
			name = name.substring(0, name.lastIndexOf(' '));
		}
		str += name.trim();
		var scoreIndex = 7;
		if (pos == " DEF")
		{
			scoreIndex = 6;
		}
		var score = cols[scoreIndex].innerText;
		str += "," + score.trim();
		var allZero = true;
		for (var j = 11; j < cols.length; j++)
		{
			var val = cols[j].innerText;
			str += "," + val.trim();
			if (parseInt(cols[j].innerText) != 0)
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
	
	var localvalue = window.sessionStorage.getItem("projdata" + pos);
	if (localvalue)
	{
		str = localvalue + str;
	}
	 window.sessionStorage.setItem("projdata" + pos, str);
	window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);
}
