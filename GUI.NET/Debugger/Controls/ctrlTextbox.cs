﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Mesen.GUI.Debugger
{
	public enum LineSymbol
	{
		None = 0,
		Circle,
		CircleOutline,
	}

	public class LineProperties
	{
		public Color? BgColor;
		public Color? FgColor;
		public Color? OutlineColor;
		public LineSymbol Symbol;
	}

	public partial class ctrlTextbox : Control
	{
		public event EventHandler ScrollPositionChanged;

		//Dictionary<int, 
		private string[] _contents = new string[0];
		private int[] _lineNumbers = new int[0];
		private Dictionary<int, int> _lineNumberIndex = new Dictionary<int,int>();
		private Dictionary<int, LineProperties> _lineProperties = new Dictionary<int,LineProperties>();
		private bool _showLineNumbers = false;
		private bool _showLineInHex = false;
		private int _cursorPosition = 0;
		private int _scrollPosition = 0;

		public ctrlTextbox()
		{
			InitializeComponent();
			this.ResizeRedraw = true;
			this.DoubleBuffered = true;
			this.Font = new Font("Consolas", 13);
		}
		
		public string[] TextLines
		{
			set
			{
				_contents = value;
				_lineNumbers = new int[_contents.Length];
				_lineNumberIndex.Clear();
				for(int i = _contents.Length - 1; i >=0; i--) {
					_lineNumbers[i] = i;
					_lineNumberIndex[i] = i;
				}
				this.Invalidate();
			}
		}

		public int LineCount
		{
			get
			{
				return _contents.Length;
			}
		}

		public int[] CustomLineNumbers
		{
			set
			{
				_lineNumbers = value;
				_lineNumberIndex.Clear();
				int i = 0;
				foreach(int line in _lineNumbers) {
					_lineNumberIndex[line] = i;
					i++;
				}
				this.Invalidate();
			}
		}

		public void ClearLineStyles()
		{
			_lineProperties.Clear();
			this.Invalidate();
		}

		public void SetLineColor(int lineNumber, Color? fgColor = null, Color? bgColor = null, Color? outlineColor = null, LineSymbol symbol = LineSymbol.None)
		{
			if(lineNumber != -1) {
				if(_lineNumberIndex.ContainsKey(lineNumber)) {
					LineProperties properties = new LineProperties() {
						BgColor = bgColor,
						FgColor = fgColor,
						OutlineColor = outlineColor,
						Symbol = symbol
					};

					_lineProperties[_lineNumberIndex[lineNumber]] = properties;
					this.Invalidate();
				}
			}
		}

		public int GetLineIndex(int lineNumber)
		{
			return _lineNumberIndex.ContainsKey(lineNumber) ? _lineNumberIndex[lineNumber] : -1;
		}

		public int GetLineNumber(int lineIndex)
		{
			return _lineNumbers[lineIndex];
		}

		public void ScrollIntoView(int lineNumber)
		{
			int lineIndex = this.GetLineIndex(lineNumber);
			if(lineIndex >= 0) {
				if(lineIndex < this.ScrollPosition || lineIndex > this.GetLastVisibleLineIndex()) {
					//Line isn't currently visible, scroll it to the middle of the viewport
					this.ScrollPosition = lineIndex - this.GetNumberVisibleLines()/2;
				}
				this.CursorPosition = lineIndex;
			}
		}

		private int GetMargin(Graphics g)
		{
			return this.ShowLineNumbers ? (int)(g.MeasureString("W", this.Font).Width * 6) : 0;
		}
		
		public string GetWordUnderLocation(Point position)
		{
			using(Graphics g = Graphics.FromHwnd(this.Handle)) {
				int marginLeft = this.GetMargin(g);
				int positionX = position.X - marginLeft;
				int lineOffset = position.Y / (this.Font.Height - 1);
				if(positionX >= 0 && this.ScrollPosition + lineOffset < _contents.Length) {
					string text = _contents[this.ScrollPosition + lineOffset];
					int charIndex = -1;
					int previousWidth = 0;
					for(int i = 0, len = text.Length; i < len; i++) {
						int width = (int)g.MeasureString(text.Substring(0, i+1), this.Font).Width;
						if(width >= positionX && previousWidth <= positionX) {
							charIndex = i;
							break;
						}
						previousWidth = width;
					}

					if(charIndex >= 0) {
						List<char> wordDelimiters = new List<char>(new char[] { ' ', ',' });
						if(wordDelimiters.Contains(text[charIndex])) {
							return string.Empty;
						} else {
							int endIndex = text.IndexOfAny(wordDelimiters.ToArray(), charIndex);
							if(endIndex == -1) {
								endIndex = text.Length;
							}
							int startIndex = text.LastIndexOfAny(wordDelimiters.ToArray(), charIndex);
							return text.Substring(startIndex + 1, endIndex - startIndex - 1);
						}
					}
				}
			}
			return string.Empty;
		}

		private int GetLastVisibleLineIndex()
		{
			return this.ScrollPosition + this.GetNumberVisibleLines() - 1;
		}

		private int GetNumberVisibleLines()
		{
			Rectangle rect = this.ClientRectangle;
			return rect.Height / (this.Font.Height - 1);
		}

		[Browsable(false), EditorBrowsable(EditorBrowsableState.Never)]
		public int CursorPosition
		{
			get { return _cursorPosition; }
			set
			{ 
				_cursorPosition = Math.Min(this._contents.Length - 1, Math.Max(0, value));
				if(_cursorPosition < this.ScrollPosition) {
					this.ScrollPosition = _cursorPosition;
				} else if(_cursorPosition > this.GetLastVisibleLineIndex()) {
					this.ScrollPosition = _cursorPosition - this.GetNumberVisibleLines() + 1;
				}
				this.Invalidate();
			}
		}

		public int CurrentLine
		{
			get { return _lineNumbers[_cursorPosition]; }
		}

		[Browsable(false), EditorBrowsable(EditorBrowsableState.Never)]
		public int ScrollPosition
		{
			get { return _scrollPosition; }
			set 
			{
				value = Math.Max(0, Math.Min(value, this._contents.Length-1));
				_scrollPosition = value;
				if(this.ScrollPositionChanged != null) {
					ScrollPositionChanged(this, null);
				}
				this.Invalidate();
			}
		}

		public bool ShowLineNumbers
		{
			get { return _showLineNumbers; }
			set { _showLineNumbers = value; }
		}

		public bool ShowLineInHex
		{
			get { return _showLineInHex; }
			set { _showLineInHex = value; }
		}

		protected override void OnMouseDown(MouseEventArgs e)
		{
			this.Focus();
			base.OnMouseDown(e);
		}

		protected override void OnMouseClick(MouseEventArgs e)
		{
			if(e.Button == System.Windows.Forms.MouseButtons.Left) {
				int clickedLine = e.Y / (this.Font.Height - 1);
				this.CursorPosition = this.ScrollPosition + clickedLine;
			}
			base.OnMouseClick(e);
		}

		private void DrawLine(Graphics g, int currentLine, int marginLeft, int positionY)
		{
			if(this.ShowLineNumbers) {
				//Show line number
				string lineNumber = _lineNumbers[currentLine] >= 0 ? _lineNumbers[currentLine].ToString(_showLineInHex ? "X4" : "") : "..";
				float width = g.MeasureString(lineNumber, this.Font).Width;
				g.DrawString(lineNumber, this.Font, Brushes.Gray, marginLeft - width, positionY);
			}

			if(currentLine == this.CursorPosition) {
				//Highlight current line
				g.FillRectangle(Brushes.AliceBlue, marginLeft, positionY, this.ClientRectangle.Width - marginLeft, this.Font.Height-1);
			}

			Color textColor = Color.Black;
			if(_lineProperties.ContainsKey(currentLine)) {
				//Process background, foreground, outline color and line symbol
				LineProperties lineProperties = _lineProperties[currentLine];
				textColor = lineProperties.FgColor ?? Color.Black;

				float stringLength = g.MeasureString(_contents[currentLine], this.Font).Width;

				if(lineProperties.BgColor.HasValue) {
					using(Brush bgBrush = new SolidBrush(lineProperties.BgColor.Value)) {
						g.FillRectangle(bgBrush, marginLeft + 1, positionY + 1, stringLength, this.Font.Height-2);
					}
				}
				if(lineProperties.OutlineColor.HasValue) {
					using(Pen outlinePen = new Pen(lineProperties.OutlineColor.Value, 1)) {
						g.DrawRectangle(outlinePen, marginLeft + 1, positionY + 1, stringLength, this.Font.Height-2);
					}
				}

				switch(lineProperties.Symbol) {
					case LineSymbol.Circle:
						using(Brush brush = new SolidBrush(lineProperties.OutlineColor.Value)) {
							g.FillEllipse(brush, 2, positionY + 3, this.Font.Height - 7, this.Font.Height - 7);
							if(lineProperties.OutlineColor.HasValue) {
								using(Pen pen = new Pen(lineProperties.OutlineColor.Value, 1)) {
									g.DrawEllipse(pen, 2, positionY + 3, this.Font.Height - 7, this.Font.Height - 7);
								}
							}
						}
						break;
					case LineSymbol.CircleOutline:
						if(lineProperties.OutlineColor.HasValue) {
							using(Pen pen = new Pen(lineProperties.OutlineColor.Value, 1)) {
								g.DrawEllipse(pen, 2, positionY + 3, this.Font.Height - 7, this.Font.Height - 7);
							}
						}
						break;
				}
			}

			using(Brush fgBrush = new SolidBrush(textColor)) {
				g.DrawString(_contents[currentLine], this.Font, fgBrush, marginLeft, positionY);
			}
		}

		protected override void OnPaint(PaintEventArgs pe)
		{
			pe.Graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
			using(Brush lightGrayBrush = new SolidBrush(Color.FromArgb(240,240,240))) {
				using(Pen grayPen = new Pen(Color.LightGray)) {
					Rectangle rect = this.ClientRectangle;
					pe.Graphics.FillRectangle(Brushes.White, rect);

					int marginLeft = this.GetMargin(pe.Graphics);
					if(this.ShowLineNumbers) {
						pe.Graphics.FillRectangle(lightGrayBrush, 0, 0, marginLeft, rect.Bottom);
						pe.Graphics.DrawLine(grayPen, marginLeft, rect.Top, marginLeft, rect.Bottom);
					}

					int currentLine = this.ScrollPosition;
					int positionY = 0;
					while(positionY < rect.Bottom && currentLine < _contents.Length) {
						this.DrawLine(pe.Graphics, currentLine, marginLeft, positionY);
						positionY += this.Font.Height - 1;
						currentLine++;
					}
				}
			}
		}
	}
}
