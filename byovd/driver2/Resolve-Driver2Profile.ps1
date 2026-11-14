function Resolve-Driver2Profile {
    param([string]$Profile)
    if (-not $Profile) { return 'rust' }
    switch ($Profile.ToLower()) {
        { $_ -in @('eac', 'eac-rust', 'eos', 'fortnite') } { return 'rust' }
        { $_ -in @('be', 'be-r6', 'r6s', 'siege', 'battleye') } { return 'r6' }
        { $_ -in @('be-ark', 'be-arksa', 'ark') } { return 'arksa' }
        { $_ -in @('ace', 'delta', 'deltaforce', 'ace-deltaforce', 'df') } { return 'deltaforce' }
        { $_ -in @('abi', 'arena', 'ace-arena', 'ace-arenabreakout', 'arenabreakout-infinite') } { return 'arenabreakout' }
        default { return $Profile.ToLower() }
    }
}

function Test-Driver2AceProfile {
    param([string]$Profile)
    return (Resolve-Driver2Profile $Profile) -in @('deltaforce', 'arenabreakout')
}

function Test-Driver2EacProfile {
    param([string]$Profile)
    return (Resolve-Driver2Profile $Profile) -eq 'rust'
}

function Test-Driver2BattleyeProfile {
    param([string]$Profile)
    $p = Resolve-Driver2Profile $Profile
    return $p -in @('r6', 'arksa')
}
