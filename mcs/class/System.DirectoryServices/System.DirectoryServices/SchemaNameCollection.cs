/******************************************************************************
* The MIT License
* Copyright (c) 2003 Novell Inc.,  www.novell.com
* 
* Permission is hereby granted, free of charge, to any person obtaining  a copy
* of this software and associated documentation files (the Software), to deal
* in the Software without restriction, including  without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to  permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in 
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*******************************************************************************/

//
// System.DirectoryServices.SchemaNameCollection.cs
//
// Author:
//   Sunil Kumar (sunilk@novell.com)
//   Raja R Harinath <rharinath@novell.com>
//
// Copyright (C) 2003, 2004  Novell Inc.
//

using System.Collections;

namespace System.DirectoryServices
{
	
	/// <summary>
	///Contains a list of the schema names that the
	/// SchemaFilter property of a DirectoryEntries
	///  object can use.
	/// </summary>
	public class SchemaNameCollection : CollectionBase
	{
		internal SchemaNameCollection ()
		{
		}
		
		public int Add (string value)
		{
			return List.Add (value);
		}

		public string this[int pos]
		{
			get { return List[pos] as string; }
			set { List[pos] = value; }
		}

		public int IndexOf (string s)
		{
			return List.IndexOf (s);
		}

		public bool Contains (string s)
		{
			return List.Contains (s);
		}

		public void AddRange (string[] coll)
		{
			foreach (string s in coll)
				Add (s);
		}

		public void AddRange (SchemaNameCollection coll)
		{
			foreach (string s in coll)
				Add (s);
		}

		public void Insert (int pos, string s)
		{
			List.Insert (pos, s);
		}

		public void CopyTo (string[] copy_to, int index)
		{
			foreach (string s in List)
				copy_to[index++] = s;
		}

		public void Remove (string s)
		{
			List.Remove (s);
		}
	}
}

