using System;

class X {
	static int Main ()
	{
		var j = ["foo":1, "bar":2];

		if (j.GetType ().ToString () != ("System.Collections.Generic.Dictionary`2[System.String,System.Int32]"))
			return 1;
		    
		if (j.Count != 2)
			return 2;

		if (j ["foo"] != 1)
			return 3;

		if (j ["bar"] != 2)
			return 4;

		var k = ["foo":[1:2,3:4],"bar":[5:6,7:8]];

		if (k.GetType ().ToString () != "System.Collections.Generic.Dictionary`2[System.String,System.Collections.Generic.Dictionary`2[System.Int32,System.Int32]]")
			return 7;

		return 0;
	}
}
