import unittest
from uv_validation import validate

def triangle(uv, offset=0, indices=None):
    return {'uv':uv,'xyz':[(0,0,0),(1,0,0),(0,1,0)],
            'vertices':indices or [offset,offset+1,offset+2]}

class UVValidationTests(unittest.TestCase):
    def check(self, triangles):
        return validate(triangles, resolution=2048, gap_px=32, outer_px=16)

    def test_shared_edge_is_allowed(self):
        a=triangle([(.1,.1),(.4,.1),(.1,.4)],indices=[0,1,2])
        b=triangle([(.4,.1),(.4,.4),(.1,.4)],indices=[1,3,2])
        result=self.check([a,b])
        self.assertEqual(result['overlap_pairs'],[])
        self.assertEqual(result['island_count'],1)

    def test_overlapping_interiors_fail_even_with_different_indices(self):
        uv=[(.1,.1),(.4,.1),(.1,.4)]
        self.assertIn('overlap',self.check([triangle(uv),triangle(uv,3)])['errors'])

    def test_degenerate_and_outside_fail(self):
        self.assertIn('degenerate_uv',self.check([triangle([(.1,.1)]*3)])['errors'])
        self.assertIn('outside_atlas',self.check([triangle([(-.1,.1),(.4,.1),(.1,.4)])])['errors'])

    def test_density_imbalance_fails(self):
        triangles=[]
        for i,(x,y,scale) in enumerate([(.05,.05,1),(.5,.05,1),(.05,.5,1),(.5,.5,1.2)]):
            triangles.append(triangle([(x,y),(x+.2*scale,y),(x,y+.2*scale)],i*3))
        self.assertIn('density',self.check(triangles)['errors'])

    def test_31_pixel_gap_and_15_pixel_border_fail(self):
        a=triangle([(.1,.1),(.3,.1),(.1,.3)])
        d=31/2048
        b=triangle([(.3+d,.1),(.5+d,.1),(.3+d,.3)],3)
        self.assertIn('island_margin',self.check([a,b])['errors'])
        c=triangle([(15/2048,.1),(.3,.1),(.1,.3)])
        self.assertIn('outer_margin',self.check([c])['errors'])

if __name__=='__main__': unittest.main()
