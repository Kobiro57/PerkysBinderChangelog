"""Verify the published r19 viewer and release metadata without using a credential."""
from pathlib import Path
import json,posixpath,re,urllib.request
R=Path(__file__).resolve().parents[1]
BASE='https://api.github.com/repos/Kobiro57/PerkysBinderChangelog'

def get(path):
    request=urllib.request.Request(BASE+path,headers={'User-Agent':'NLBinder-r19-anonymous-verification',
        'Accept':'application/vnd.github+json'})
    with urllib.request.urlopen(request,timeout=45) as response:return json.load(response)

def require(ok,message):
    if not ok:raise ValueError(message)

def main():
    publication=json.loads((R/'build/revision19-published.json').read_text(encoding='utf-8'))
    proof=json.loads((R/'build/revision19-package.json').read_text(encoding='utf-8'))
    require(get('')['private'] is False,'Repository is not anonymously readable')
    require(get('/commits/main')['sha']==publication['commit'],'Remote main differs')
    release=get('/releases/tags/v0.1.1-r19')
    asset=next(a for a in release['assets'] if a['name']==proof['archive'])
    require(asset['digest']=='sha256:'+proof['archive_sha256'] and asset['size']==proof['archive_bytes'],
            'Release asset digest or size differs')
    repository=R/'build/github-publication/repository'
    for name in ['README.md','docs/NLBinder-0.1.1-r19-DE.md','docs/review/r19-PRUEFUNG-DE.md']:
        for href in re.findall(r'\]\(([^)]+)\)',(repository/name).read_text(encoding='utf-8-sig')):
            if href.startswith(('http:','https:','#')):continue
            path=posixpath.normpath(posixpath.join(posixpath.dirname(name),href.split('#')[0]))
            require((repository/path).exists(),'Broken review link: '+name+' -> '+href)
    result={'anonymous_viewer_access':True,'main_commit_verified':True,'release_asset_sha256_verified':True,
        'review_document_links_verified':True,'release_url':release['html_url']}
    (R/'build/revision19-public-access.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
