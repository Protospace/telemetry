$postParams = @{name=$env:computername ;username=$env:UserName}

while($true)
{
	if (!([System.Diagnostics.Process]::GetProcessesByName("logonui")))
	{
		# screen is unlocked

		# Invoke-WebRequest -Uri https://api.spaceport.dns.t0.vc/stats/track/ -Method POST -Body $postParams -ContentType application/x-www-form-urlencoded -UseBasicParsing
		Invoke-WebRequest -Uri https://api.my.protospace.ca/stats/track/ -Method POST -Body $postParams -ContentType application/x-www-form-urlencoded -UseBasicParsing
	}

	Start-Sleep -s 10
}
