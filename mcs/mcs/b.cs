using System;
using System.Collections;

class X {
	static void Main ()
	{
		Hashtable j = ["foo":"bar"];
		Console.WriteLine ("The type is: {0}", j);

		var k = ["work":new Uri ("http://microsoft.com"),
			 "leisure":new Uri ("http://catoverflow.com")];
			
		foreach (var kp in j.Keys){
			Console.WriteLine ($"k={kp} v={j[kp]}");
		}
	}
}
