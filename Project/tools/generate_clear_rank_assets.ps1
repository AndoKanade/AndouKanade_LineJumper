# クリア画面で押すランクの文字(S / A / B / C)と、その上に出す「RANK」の画像を生成するスクリプト(仮素材)
# 使い方: powershell -ExecutionPolicy Bypass -File tools/generate_clear_rank_assets.ps1  (Projectフォルダで実行する)
# 出力先: resource/UI/clearRank/rankS.png / rankA.png / rankB.png / rankC.png / rankLabel.png
# ランクの色はゲーム側で文字の後ろのインクに付けるため、文字は白で塗り、濃い紺で縁取る。
# Python が無い環境でも作れるよう、Windows に入っている System.Drawing で描く。
# Windows PowerShell 5.1 で日本語のコメントを読めるよう、このファイルは BOM 付きの UTF-8 で保存する。

Add-Type -AssemblyName System.Drawing

# 出力先のフォルダと、文字に使うフォント(リポジトリに入っている丸みのある日本語フォント)
$OutputDir = Join-Path (Get-Location) "resource/UI/clearRank"
$FontPath = Join-Path (Get-Location) "resource/fonts/MochiyPopPOne-Regular.ttf"

# ランクの文字の画像の大きさ(ピクセル)・文字の大きさ・縁取りの太さ
$RankImageSize = 256
$RankFontSize = 170
$RankOutlineWidth = 18
# 「RANK」の画像の大きさ(ピクセル)・文字の大きさ・縁取りの太さ
$LabelImageWidth = 320
$LabelImageHeight = 96
$LabelFontSize = 60
$LabelOutlineWidth = 12
# 右へ傾ける量(斜体。tools/generate_stage_clear_assets.py の SHEAR と同じ)
$Shear = 0.18

# 色。縁取りは tools/generate_stage_clear_assets.py の DARK_NAVY と同じ
$FillColor = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)
$OutlineColor = [System.Drawing.Color]::FromArgb(255, 20, 34, 48)

# 書き出すランクの文字とファイル名
$Ranks = @(
    @{ Text = "S"; File = "rankS.png" },
    @{ Text = "A"; File = "rankA.png" },
    @{ Text = "B"; File = "rankB.png" },
    @{ Text = "C"; File = "rankC.png" }
)

# フォントファイルを読み込む(インストールしていなくても使えるようにする)
$FontCollection = New-Object System.Drawing.Text.PrivateFontCollection
$FontCollection.AddFontFile($FontPath)
$FontFamily = $FontCollection.Families[0]

# 文字を画像の中央に、縁取り付き・斜体で描いて書き出す
function Write-OutlinedText([string]$text, [int]$width, [int]$height, [float]$fontSize, [float]$outlineWidth, [string]$fileName){
    $bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)

    # 文字の形を作り、画像の中央へ動かしてから右へ傾ける
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddString($text, $FontFamily, [int][System.Drawing.FontStyle]::Regular, $fontSize, (New-Object System.Drawing.PointF(0, 0)), [System.Drawing.StringFormat]::GenericTypographic)
    $bounds = $path.GetBounds()
    $centerMatrix = New-Object System.Drawing.Drawing2D.Matrix
    $centerMatrix.Translate(($width - $bounds.Width) / 2 - $bounds.X, ($height - $bounds.Height) / 2 - $bounds.Y)
    $path.Transform($centerMatrix)
    $shearMatrix = New-Object System.Drawing.Drawing2D.Matrix(1, 0, (-$Shear), 1, ($Shear * $height / 2), 0)
    $path.Transform($shearMatrix)

    # 縁取りを先に太く描き、その上を白で塗る
    $pen = New-Object System.Drawing.Pen($OutlineColor, ($outlineWidth * 2))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawPath($pen, $path)
    $brush = New-Object System.Drawing.SolidBrush($FillColor)
    $graphics.FillPath($brush, $path)

    $bitmap.Save((Join-Path $OutputDir $fileName), [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output "生成しました: $(Join-Path $OutputDir $fileName)"

    $brush.Dispose()
    $pen.Dispose()
    $path.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}

New-Item -ItemType Directory -Force $OutputDir | Out-Null
foreach($rank in $Ranks){
    Write-OutlinedText $rank.Text $RankImageSize $RankImageSize $RankFontSize $RankOutlineWidth $rank.File
}
Write-OutlinedText "RANK" $LabelImageWidth $LabelImageHeight $LabelFontSize $LabelOutlineWidth "rankLabel.png"
