using SevsModManager.Theme;
using SevsModManager.UI.Controls;
using SevsModManager.Core;

namespace SevsModManager.UI;

internal sealed class ModpackExportDialog : Form
{
    public string PackName => _nameBox.Text.Trim();
    public List<string> IncludedPaths { get; private set; } = new();

    private readonly RTextBox _nameBox;
    private readonly TreeView _tree;

    public ModpackExportDialog(List<PackRoot> roots, string defaultName)
    {
        var t = ThemeEngine.Current;

        Text = "Create Modpack";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        ClientSize = new Size(440, 520);
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = false;
        BackColor = t.Background;

        var nameLbl = new Label
        {
            Text = "Modpack name:", AutoSize = true, Location = new Point(16, 14), ForeColor = t.Text,
        };
        _nameBox = new RTextBox
        {
            Location = new Point(16, 36), Width = 408, Height = 30,
            BackColor = t.SurfaceAlt, ForeColor = t.Text, CornerRadius = 6,
            Text = defaultName,
        };

        string defaultsText = string.Join(" and ", ModpackManager.DefaultRootNames);
        var treeLbl = new Label
        {
            Text = $"Files to include ({defaultsText} are included by default):",
            AutoSize = true, Location = new Point(16, 76), ForeColor = t.SubText, Font = new Font("Segoe UI", 8.5f),
        };

        _tree = new TreeView
        {
            Location = new Point(16, 98), Width = 408, Height = 366,
            CheckBoxes = true, BackColor = t.Surface, ForeColor = t.Text, BorderStyle = BorderStyle.FixedSingle,
        };
        _tree.AfterCheck += Tree_AfterCheck;
        _tree.HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(_tree);

        var cancelBtn = new RButton { Text = "Cancel", Width = 100, Height = 32, Location = new Point(16, 472) };
        var saveBtn   = new RButton { Text = "Save",   Width = 100, Height = 32, Location = new Point(324, 472) };
        ThemeEngine.StyleRButton(cancelBtn);
        ThemeEngine.StyleRButton(saveBtn, accent: true);
        cancelBtn.Click += (_, __) => { DialogResult = DialogResult.Cancel; Close(); };
        saveBtn.Click   += (_, __) => Confirm();

        Controls.Add(nameLbl);
        Controls.Add(_nameBox);
        Controls.Add(treeLbl);
        Controls.Add(_tree);
        Controls.Add(cancelBtn);
        Controls.Add(saveBtn);

        PopulateTree(roots);
        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        Shown += (_, __) => { _nameBox.Inner.Focus(); _nameBox.Inner.SelectAll(); };
    }

    private void PopulateTree(List<PackRoot> roots)
    {
        _tree.Nodes.Clear();
        var defaults = ModpackManager.DefaultRootNames;

        if (roots.Count == 1)
        {
            var root = roots[0];
            if (!Directory.Exists(root.AbsolutePath)) return;

            foreach (var dir in Directory.GetDirectories(root.AbsolutePath).OrderBy(d => d))
            {
                string name = Path.GetFileName(dir);
                bool defaultChecked = defaults.Contains(name);
                var node = new TreeNode(name) { Tag = dir, Checked = defaultChecked };
                PopulateChildren(node, dir, defaultChecked);
                _tree.Nodes.Add(node);
            }
            foreach (var file in Directory.GetFiles(root.AbsolutePath).OrderBy(f => f))
                _tree.Nodes.Add(new TreeNode(Path.GetFileName(file)) { Tag = file, Checked = false });
        }
        else
        {
            foreach (var root in roots)
            {
                if (!Directory.Exists(root.AbsolutePath)) continue;

                bool defaultChecked = defaults.Contains(root.Name);
                var node = new TreeNode(root.Name) { Tag = root.AbsolutePath, Checked = defaultChecked };
                PopulateChildren(node, root.AbsolutePath, defaultChecked);
                _tree.Nodes.Add(node);
            }
        }
    }

    private static void PopulateChildren(TreeNode parent, string dirPath, bool defaultChecked)
    {
        foreach (var dir in Directory.GetDirectories(dirPath).OrderBy(d => d))
        {
            var node = new TreeNode(Path.GetFileName(dir)) { Tag = dir, Checked = defaultChecked };
            PopulateChildren(node, dir, defaultChecked);
            parent.Nodes.Add(node);
        }
        foreach (var file in Directory.GetFiles(dirPath).OrderBy(f => f))
            parent.Nodes.Add(new TreeNode(Path.GetFileName(file)) { Tag = file, Checked = defaultChecked });
    }

    private void Tree_AfterCheck(object? sender, TreeViewEventArgs e)
    {
        if (e.Action == TreeViewAction.Unknown || e.Node == null) return;
        SetChildrenChecked(e.Node, e.Node.Checked);
    }

    private static void SetChildrenChecked(TreeNode node, bool value)
    {
        foreach (TreeNode child in node.Nodes)
        {
            child.Checked = value;
            SetChildrenChecked(child, value);
        }
    }

    private void Confirm()
    {
        if (string.IsNullOrWhiteSpace(_nameBox.Text))
        {
            MessageBox.Show("Enter a name for the modpack.", "Name Required", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var included = new List<string>();
        CollectCheckedFiles(_tree.Nodes, included);
        if (included.Count == 0)
        {
            MessageBox.Show("Select at least one file to include.", "Nothing Selected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        IncludedPaths = included;
        DialogResult = DialogResult.OK;
        Close();
    }

    private static void CollectCheckedFiles(TreeNodeCollection nodes, List<string> result)
    {
        foreach (TreeNode node in nodes)
        {
            if (node.Nodes.Count == 0)
            {
                if (node.Checked && node.Tag is string path) result.Add(path);
            }
            else
            {
                CollectCheckedFiles(node.Nodes, result);
            }
        }
    }
}
