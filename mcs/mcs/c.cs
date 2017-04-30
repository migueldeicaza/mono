using System;
using System.Collections;

class X {
	static void Main ()
	{
		var j = (Hashtable)  ["foo":"bar"];

		var k = ["work":new Uri ("http://microsoft.com"),
			 "leisure":new Uri ("http://catoverflow.com")];
			
		foreach (var kp in j.Keys){
			Console.WriteLine ($"k={kp} v={j[kp]}");
		}
	}
}
