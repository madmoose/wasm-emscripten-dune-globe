using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;

namespace accessors
{
	class Program
	{
		static void Main(string[] args)
		{
			{
				// better, ai based rainbow color-map
				// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
				// https://gist.github.com/mikhailov-work/6a308c20e494d9e0ccc29036b28faa7a
				// https://gist.github.com/mikhailov-work/ee72ba4191942acecc03fe6da94fc73f
				// https://gist.github.com/mikhailov-work/ee72ba4191942acecc03fe6da94fc73f

				var color_bitmap = new Bitmap(@"F:\projects\fun\dos_games_rev\dune_scummvm_cryo_stuff\wasm-emscripten-dune-globe\tests\google_ai_turbo_rainbow_colors.png");

				//--------

				//for (int x = 0; x < color_bitmap.Width; x++)
				//	for (int y = 0; y < color_bitmap.Height; y++)
				//	{
				//		Color pixelColor = color_bitmap.GetPixel(x, y);

				//		System.Drawing.ColorTranslator.ToHtml(pixelColor);

				//		// things we do with pixelColor
				//	}

				//-------

				using StreamWriter colormap = new(@"F:\projects\fun\dos_games_rev\dune_scummvm_cryo_stuff\wasm-emscripten-dune-globe\tests\colormap.html");

				string table_style = "<style type=\"text/css\">td{font-family: Arial; font-size: 14px; height: 14px; width: 14px; border:none}</style>";

				colormap.WriteLine(table_style);

				//-----------

				//colormap.WriteLine("<table>");

				//colormap.WriteLine("  <tr>");
				//for (int i = 0; i < 256; ++i)
				//{
				//	Color pixelColor = color_bitmap.GetPixel(i*2, 0);
				//	string html_color = System.Drawing.ColorTranslator.ToHtml(pixelColor);

				//	colormap.WriteLine("    <td bgcolor=\"{0}\"></td>", html_color);
				//}
				//colormap.WriteLine("  </tr>");

				//colormap.WriteLine("</table>");

				//---------------

				{
					colormap.WriteLine("<table cellspacing=\"0\">");


#if true
					// cos/sin tables?
#if true
					int xwidth = 200;
					int file_start = 3290;
					int length = 64 * 200;
#else
					int xwidth = 63;
					int file_start = 0;
					int length = 3290;
#endif
#else
					int file_start = 0;
					int length = globedata.data.Length;
#endif
					int xfull_lines = length / xwidth;
					int xrest = length % xwidth;

					int xlines = xfull_lines;
					if (xrest != 0)
					{
						++xlines;
					}

					for (int i = 0; i < xlines; ++i)
					{
						int start = i * xwidth;

						colormap.WriteLine("  <tr>");

						for (int p = 0; p < xwidth; ++p)
						{
							int curr = start + p;

							if (curr < length)
							{
								//Debug.Assert((curr >= file_start) && (curr <= file_start + length));

								var byte_value = globedata.data[file_start+curr];

								string html_color;
								if (byte_value == 0)
								{
									html_color = "grey";
								}
								else if(byte_value == 255)
								{
									html_color = "white";
								}
								else
								{ 
									Color pixelColor = color_bitmap.GetPixel(byte_value * 2, 0);
									html_color = System.Drawing.ColorTranslator.ToHtml(pixelColor);
								}

								colormap.WriteLine("   <td bgcolor=\"{0}\" title=\"{1}:{2}\"></td>", html_color, curr.ToString("X4"), byte_value.ToString("X2"));
							}
							else
							{
								colormap.WriteLine("   <td style=\"text-align:center\">-</td>");
							}
						}

						colormap.WriteLine("  </tr>");
					}

					colormap.WriteLine("</table>");
				}
			}

			string filename = @"F:\projects\fun\dos_games_rev\dune_scummvm_cryo_stuff\wasm-emscripten-dune-globe\tests\accessors.txt";

			var file = File.ReadAllLines(filename);

			var offsets = new List<string> { };
			var color_dict = new Dictionary<string,int> { };

			foreach (var line in file)
			{
				string[] subs = line.Split(':');

				int offset = int.Parse(subs[0]);

				string color_key = subs[1].Trim();

				offsets.Add(color_key);
				if (color_dict.ContainsKey(color_key))
				{
					++color_dict[color_key];
				}
				else
				{
					color_dict[color_key] = 1;
				}
			}

			Debug.Assert(color_dict.ContainsKey("unused"));
			Debug.Assert(color_dict.ContainsKey("1,5"));
			Debug.Assert(color_dict.ContainsKey("0,5"));
			Debug.Assert(color_dict.ContainsKey("5"));
			Debug.Assert(color_dict.ContainsKey("0,2,5"));
			Debug.Assert(color_dict.ContainsKey("0"));
			Debug.Assert(color_dict.ContainsKey("3"));
			Debug.Assert(color_dict.ContainsKey("4"));

			var color_html = new Dictionary<string, string> 
			{
				{ "unused","black" },
				{ "1,5","green" },
				{ "0,5","yellow" },
				{ "5","fuchsia" },
				{ "0,2,5","lime" },
				{ "0","blue" },
				{ "3","teal" },
				{ "4","aqua" }
			};

			using StreamWriter html = new(@"F:\projects\fun\dos_games_rev\dune_scummvm_cryo_stuff\wasm-emscripten-dune-globe\tests\accessors.html");

			string style = "<style type=\"text/css\">td{font-family: Arial; font-size: 12px; height: 12px; width: 12px}</style>";

			html.WriteLine(style);

			html.WriteLine("<table>");
			
			html.WriteLine("  <tr>");
			html.WriteLine("    <td>key</td>");
			html.WriteLine("    <td>count</th>");
			html.WriteLine("    <td>color</th>");
			html.WriteLine("  </tr>");

			foreach (var color in color_dict)
			{
				html.WriteLine("  <tr>");
				html.WriteLine("    <td>{0}</td>", color.Key.ToString());
				html.WriteLine("    <td>{0}</th>", color.Value.ToString());
				html.WriteLine("    <td bgcolor=\"{0}\"></th>", color_html[color.Key]);
				html.WriteLine("  </tr>");
			}

			html.WriteLine("</table>");

			html.WriteLine("<table>");

			int width = 64;

			int full_lines = offsets.Count / width;
			int rest = offsets.Count % width;

			int lines = full_lines;
			if (rest != 0)
			{
				++lines;
			}

			for (int i = 0; i < lines; ++i)
			{
				int start = i * width;

				html.WriteLine("  <tr>");
				html.WriteLine("    <td>{0}</td>", start.ToString("X4"));

				for (int p = 0; p < width; ++p)
				{
					int curr = start + p;

					if (curr < offsets.Count)
					{
						var color_key = offsets[curr];
						var html_color = color_html[color_key];

						html.WriteLine("   <td bgcolor=\"{0}\" title=\"{1}:{2}\"></td>", html_color, curr.ToString("X4"), color_key);
					}
					else
					{
						html.WriteLine("   <td style=\"text-align:center\">-</td>");
					}
				}

				html.WriteLine("  </tr>");
			}

			html.WriteLine("</table>");
		}
	}
}

