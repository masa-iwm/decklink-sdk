namespace SignalGenCSharp
{
    partial class SignalGenerator
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.label6 = new System.Windows.Forms.Label();
            this.label5 = new System.Windows.Forms.Label();
            this.comboBoxPixelFormat = new System.Windows.Forms.ComboBox();
            this.comboBoxOutputDevice = new System.Windows.Forms.ComboBox();
            this.comboBoxVideoFormat = new System.Windows.Forms.ComboBox();
            this.label4 = new System.Windows.Forms.Label();
            this.comboBoxAudioDepth = new System.Windows.Forms.ComboBox();
            this.label3 = new System.Windows.Forms.Label();
            this.comboBoxAudioChannels = new System.Windows.Forms.ComboBox();
            this.label2 = new System.Windows.Forms.Label();
            this.comboBoxOutputSignal = new System.Windows.Forms.ComboBox();
            this.label1 = new System.Windows.Forms.Label();
            this.buttonStartStop = new System.Windows.Forms.Button();
            this.groupBox2 = new System.Windows.Forms.GroupBox();
            this.previewWindow = new SignalGenCSharp.PreviewWindow();
            this.groupBox1.SuspendLayout();
            this.groupBox2.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.label6);
            this.groupBox1.Controls.Add(this.label5);
            this.groupBox1.Controls.Add(this.comboBoxPixelFormat);
            this.groupBox1.Controls.Add(this.comboBoxOutputDevice);
            this.groupBox1.Controls.Add(this.comboBoxVideoFormat);
            this.groupBox1.Controls.Add(this.label4);
            this.groupBox1.Controls.Add(this.comboBoxAudioDepth);
            this.groupBox1.Controls.Add(this.label3);
            this.groupBox1.Controls.Add(this.comboBoxAudioChannels);
            this.groupBox1.Controls.Add(this.label2);
            this.groupBox1.Controls.Add(this.comboBoxOutputSignal);
            this.groupBox1.Controls.Add(this.label1);
            this.groupBox1.Location = new System.Drawing.Point(12, 12);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(260, 207);
            this.groupBox1.TabIndex = 0;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Properties";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(23, 165);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(64, 13);
            this.label6.TabIndex = 11;
            this.label6.Text = "Pixel Format";
            this.label6.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(11, 30);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(76, 13);
            this.label5.TabIndex = 10;
            this.label5.Text = "Output Device";
            this.label5.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // comboBoxPixelFormat
            // 
            this.comboBoxPixelFormat.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxPixelFormat.FormattingEnabled = true;
            this.comboBoxPixelFormat.Location = new System.Drawing.Point(93, 162);
            this.comboBoxPixelFormat.Name = "comboBoxPixelFormat";
            this.comboBoxPixelFormat.Size = new System.Drawing.Size(141, 21);
            this.comboBoxPixelFormat.TabIndex = 9;
            this.comboBoxPixelFormat.SelectedIndexChanged += new System.EventHandler(this.comboBoxPixelFormat_SelectedIndexChanged);
            // 
            // comboBoxOutputDevice
            // 
            this.comboBoxOutputDevice.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxOutputDevice.FormattingEnabled = true;
            this.comboBoxOutputDevice.Location = new System.Drawing.Point(93, 27);
            this.comboBoxOutputDevice.Name = "comboBoxOutputDevice";
            this.comboBoxOutputDevice.Size = new System.Drawing.Size(141, 21);
            this.comboBoxOutputDevice.TabIndex = 8;
            this.comboBoxOutputDevice.SelectedIndexChanged += new System.EventHandler(this.comboBoxOutputDevice_SelectedValueChanged);
            // 
            // comboBoxVideoFormat
            // 
            this.comboBoxVideoFormat.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxVideoFormat.FormattingEnabled = true;
            this.comboBoxVideoFormat.Location = new System.Drawing.Point(93, 135);
            this.comboBoxVideoFormat.Name = "comboBoxVideoFormat";
            this.comboBoxVideoFormat.Size = new System.Drawing.Size(141, 21);
            this.comboBoxVideoFormat.TabIndex = 7;
            this.comboBoxVideoFormat.SelectedIndexChanged += new System.EventHandler(this.comboBoxVideoFormat_SelectedIndexChanged);
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(18, 138);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(69, 13);
            this.label4.TabIndex = 6;
            this.label4.Text = "Video Format";
            this.label4.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // comboBoxAudioDepth
            // 
            this.comboBoxAudioDepth.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxAudioDepth.FormattingEnabled = true;
            this.comboBoxAudioDepth.Location = new System.Drawing.Point(93, 108);
            this.comboBoxAudioDepth.Name = "comboBoxAudioDepth";
            this.comboBoxAudioDepth.Size = new System.Drawing.Size(141, 21);
            this.comboBoxAudioDepth.TabIndex = 5;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(21, 111);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(66, 13);
            this.label3.TabIndex = 4;
            this.label3.Text = "Audio Depth";
            this.label3.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // comboBoxAudioChannels
            // 
            this.comboBoxAudioChannels.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxAudioChannels.FormattingEnabled = true;
            this.comboBoxAudioChannels.Location = new System.Drawing.Point(93, 81);
            this.comboBoxAudioChannels.Name = "comboBoxAudioChannels";
            this.comboBoxAudioChannels.Size = new System.Drawing.Size(141, 21);
            this.comboBoxAudioChannels.TabIndex = 3;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 84);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(81, 13);
            this.label2.TabIndex = 2;
            this.label2.Text = "Audio Channels";
            this.label2.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // comboBoxOutputSignal
            // 
            this.comboBoxOutputSignal.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxOutputSignal.FormattingEnabled = true;
            this.comboBoxOutputSignal.Location = new System.Drawing.Point(93, 54);
            this.comboBoxOutputSignal.Name = "comboBoxOutputSignal";
            this.comboBoxOutputSignal.Size = new System.Drawing.Size(141, 21);
            this.comboBoxOutputSignal.TabIndex = 1;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(16, 57);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(71, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Output Signal";
            this.label1.TextAlign = System.Drawing.ContentAlignment.TopRight;
            // 
            // buttonStartStop
            // 
            this.buttonStartStop.Location = new System.Drawing.Point(105, 225);
            this.buttonStartStop.Name = "buttonStartStop";
            this.buttonStartStop.Size = new System.Drawing.Size(75, 23);
            this.buttonStartStop.TabIndex = 1;
            this.buttonStartStop.Text = "Start";
            this.buttonStartStop.UseVisualStyleBackColor = true;
            this.buttonStartStop.Click += new System.EventHandler(this.buttonStartStop_Click);
            // 
            // groupBox2
            // 
            this.groupBox2.Controls.Add(this.previewWindow);
            this.groupBox2.Location = new System.Drawing.Point(278, 12);
            this.groupBox2.Name = "groupBox2";
            this.groupBox2.Size = new System.Drawing.Size(333, 207);
            this.groupBox2.TabIndex = 2;
            this.groupBox2.TabStop = false;
            this.groupBox2.Text = "Output";
            // 
            // previewWindow
            // 
            this.previewWindow.Location = new System.Drawing.Point(6, 19);
            this.previewWindow.Name = "previewWindow";
            this.previewWindow.Size = new System.Drawing.Size(319, 179);
            this.previewWindow.TabIndex = 0;
            this.previewWindow.Text = "previewWindow";
            // 
            // SignalGenerator
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(617, 257);
            this.Controls.Add(this.groupBox2);
            this.Controls.Add(this.buttonStartStop);
            this.Controls.Add(this.groupBox1);
            this.Name = "SignalGenerator";
            this.Text = "SignalGenerator";
            this.Load += new System.EventHandler(this.SignalGenerator_Load);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.groupBox2.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.ComboBox comboBoxVideoFormat;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.ComboBox comboBoxAudioDepth;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.ComboBox comboBoxAudioChannels;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.ComboBox comboBoxOutputSignal;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button buttonStartStop;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.ComboBox comboBoxPixelFormat;
        private System.Windows.Forms.ComboBox comboBoxOutputDevice;
        private System.Windows.Forms.GroupBox groupBox2;
        private PreviewWindow previewWindow;
    }
}