Current work:

Trying to allow the existing patch for dictionary literals that allows:

       var a = ["key":"value"];

Which creates an inferred Dictionary<string,string> above into
allowing the use of Dictionary-shaped APIs like this:

      Hashtable a = ["key":"value"];

The pipeline is in place, what we had to do is hook up a few places
code to ensure that the assignment in that case propates the type.   The current code hits this:

     Attempting to convert a Dictionary into System.Collections.Hashtable
     Attemping to convert to System.Collections.Hashtable

And at this point, we should perform the conversion if possible.
After that you get a crash, expected, because the above just blindly
proceeds.

Working code:

```csharp
using System;

class X {
	static void Main ()
	{
		var j = ["foo":[1.2:2.3], "bar":[1.2:3.4]];

		var k = ["work":new Uri ("http://microsoft.com"),
			 "leisure":new Uri ("http://catoverflow.com")];
			
		foreach (var kp in j){
			Console.WriteLine ($"k={kp.Key} v={kp.Value}");
		}
	}
}
```

Work in progress code:

```csharp
using System;
using System.Collections;

class X {
	static void Main ()
		{
		Hashtable j = ["foo":"bar"];
		var i = (Hashtable) ["foo":"bar"];

		var k = ["work":new Uri ("http://microsoft.com"),
			 "leisure":new Uri ("http://catoverflow.com")];
			
		foreach (var kp in j.Keys){
			Console.WriteLine ($"k={kp} v={j[kp]}");
		}
	}
}
```

Compile and run with:

    make && mono --debug=casts  ../class/lib/net_4_x/mcs.exe b.cs  && mono b.exe

