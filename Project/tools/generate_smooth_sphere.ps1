# 凹凸の無いなめらかな球のモデルを生成するスクリプト
# 使い方: powershell -ExecutionPolicy Bypass -File tools/generate_smooth_sphere.ps1  (Projectフォルダで実行する)
# 出力先: resource/SmoothSphere/smoothSphere.obj と smoothSphere.mtl
# インクのしぶき・閃光・火花(InkEffectManager)で使う。
# 前の resource/Sphere/sphere.obj は面ごとの法線で、輪の間隔もそろっていないため、光を当てると角ばって見えていた。
# ここでは緯度・経度を等しい角度で分け、法線を頂点ごとに(中心から頂点への向き)持たせて、光の当たり方がなめらかに変わるようにする。
# テクスチャは前の球と同じ白一色の画像(resource/Sphere/white.png)を同じフォルダへ写して使う。

$ErrorActionPreference = "Stop"

$outputDir = "resource/SmoothSphere"
$objName = "smoothSphere.obj"
$mtlName = "smoothSphere.mtl"
$materialName = "SmoothSphere"
$textureSource = "resource/Sphere/white.png"
$textureName = "white.png"

# 半径(前の球と同じ 1 にして、InkEffectManager の大きさの値をそのまま使えるようにする)
$radius = 1.0
# 経度(横)と緯度(縦)の分割数。しぶきは画面の中で小さいため、これだけあれば輪郭も角ばって見えない
$longitudeDivision = 32
$latitudeDivision = 16
# 小数の書き方(桁数 6。地域の設定に関係なく小数点を「.」にする)
$numberFormat = "F6"
$culture = [System.Globalization.CultureInfo]::InvariantCulture

function Format-Values([double[]]$values){
	return ($values | ForEach-Object { $_.ToString($numberFormat,$culture) }) -join " "
}

New-Item -ItemType Directory -Force $outputDir | Out-Null

$positionLines = New-Object System.Collections.Generic.List[string]
$texcoordLines = New-Object System.Collections.Generic.List[string]
$normalLines = New-Object System.Collections.Generic.List[string]

# 北極から南極へ緯度の輪を並べ、それぞれの輪に経度の数 + 1 個の頂点を置く
# (経度の最初と最後は同じ位置だが、UV の端をつなぐために別の頂点にする)
for($latitudeIndex = 0; $latitudeIndex -le $latitudeDivision; $latitudeIndex++){
	$theta = [Math]::PI * $latitudeIndex / $latitudeDivision
	for($longitudeIndex = 0; $longitudeIndex -le $longitudeDivision; $longitudeIndex++){
		$phi = 2.0 * [Math]::PI * $longitudeIndex / $longitudeDivision
		$normal = @(
			([Math]::Sin($theta) * [Math]::Cos($phi)),
			[Math]::Cos($theta),
			([Math]::Sin($theta) * [Math]::Sin($phi))
		)
		$normalLines.Add("vn " + (Format-Values $normal))
		$positionLines.Add("v " + (Format-Values @(($normal[0] * $radius),($normal[1] * $radius),($normal[2] * $radius))))
		$texcoordLines.Add("vt " + (Format-Values @(($longitudeIndex / $longitudeDivision),(1.0 - $latitudeIndex / $latitudeDivision))))
	}
}

# obj の番号は 1 から数える
function Get-VertexIndex([int]$latitudeIndex,[int]$longitudeIndex){
	return $latitudeIndex * ($longitudeDivision + 1) + $longitudeIndex + 1
}

function Format-Face([int[]]$indices){
	return "f " + (($indices | ForEach-Object { "$_/$_/$_" }) -join " ")
}

# 隣り合う2つの輪の間を三角形でつなぐ(極では面積の無い三角形になるため、1つだけにする)
$faceLines = New-Object System.Collections.Generic.List[string]
for($latitudeIndex = 0; $latitudeIndex -lt $latitudeDivision; $latitudeIndex++){
	for($longitudeIndex = 0; $longitudeIndex -lt $longitudeDivision; $longitudeIndex++){
		$topLeft = Get-VertexIndex $latitudeIndex $longitudeIndex
		$topRight = Get-VertexIndex $latitudeIndex ($longitudeIndex + 1)
		$bottomLeft = Get-VertexIndex ($latitudeIndex + 1) $longitudeIndex
		$bottomRight = Get-VertexIndex ($latitudeIndex + 1) ($longitudeIndex + 1)
		if($latitudeIndex -ne 0){
			$faceLines.Add((Format-Face @($topLeft,$topRight,$bottomLeft)))
		}
		if($latitudeIndex -ne ($latitudeDivision - 1)){
			$faceLines.Add((Format-Face @($topRight,$bottomRight,$bottomLeft)))
		}
	}
}

$objLines = @(
	"# tools/generate_smooth_sphere.ps1 で生成",
	"mtllib $mtlName",
	"o SmoothSphere"
) + $positionLines + $texcoordLines + $normalLines + @(
	"usemtl $materialName",
	"s 1"
) + $faceLines

$mtlLines = @(
	"# tools/generate_smooth_sphere.ps1 で生成",
	"newmtl $materialName",
	"Ka 1.000000 1.000000 1.000000",
	"Kd 1.000000 1.000000 1.000000",
	"Ks 0.000000 0.000000 0.000000",
	"d 1.000000",
	"illum 1",
	"map_Kd $textureName"
)

# BOM なしの UTF-8 で書き出す(モデルの読み込みで先頭の BOM を文字として読まないようにする)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$fullOutputDir = (Resolve-Path $outputDir).Path
[System.IO.File]::WriteAllText((Join-Path $fullOutputDir $objName),(($objLines -join "`n") + "`n"),$utf8NoBom)
[System.IO.File]::WriteAllText((Join-Path $fullOutputDir $mtlName),(($mtlLines -join "`n") + "`n"),$utf8NoBom)
Copy-Item $textureSource (Join-Path $outputDir $textureName) -Force

Write-Output "$outputDir/$objName : 頂点 $($positionLines.Count)、三角形 $($faceLines.Count)"
