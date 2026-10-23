import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('update_mapeval', Path(__file__).resolve().parents[1] / 'tools/update_mapeval.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class CatalogGenerationTests(unittest.TestCase):
    def test_preserves_empty_and_negative_values(self):
        root, rows = mod.parse_kv('"mapEval" { "cp" { "cp_a" "" "cp_b" "-1" } }')
        self.assertEqual(root, 'mapEval')
        self.assertEqual(rows[0][1], [('cp_a', ''), ('cp_b', '-1')])

    def test_groups_descending_count(self):
        _, groups = mod.parse_kv(mod.render({'small': [('a', '')], 'large': [('b', '2'), ('c', '')]}, ['b']))
        self.assertEqual([g for g, _ in groups], ['large', 'small', 'popular maps'])

    def test_payload_penalty_before_top50(self):
        names = {f'koth_{i}': '' for i in range(55)}
        names['pl_high'] = ''
        groups = {n: 'koth' for n in names}
        groups['pl_high'] = 'payload'
        rows = [{'map_name': n, 'player_hours': 90-i, 'avg_players': 10, 'sessions': 1} for i, n in enumerate(names) if n != 'pl_high']
        rows += [{'map_name': 'pl_high', 'player_hours': 100, 'avg_players': 20, 'sessions': 1}]
        ranking, scores = mod.rank_popular(rows, names, groups)
        self.assertEqual(len(ranking), 50)
        self.assertEqual(len(set(ranking)), 50)
        self.assertEqual(ranking[0], 'koth_0')
        self.assertEqual(scores['pl_high'][0], 70)
        self.assertGreater(ranking.index('pl_high'), 0)

    def test_workshop_popularity_alias_deduplication(self):
        names = {'workshop/cp_steel.ugc123': '2', 'cp_steel': ''}
        rows = [{'map_name': n, 'player_hours': 50, 'avg_players': 10} for n in names]
        ranked, _ = mod.rank_popular(rows, names, dict.fromkeys(names, 'attack/defend'))
        self.assertEqual(len(ranked), 1)

    def test_actual_cp_entities_override_prefix(self):
        for count in (3, 5):
            points = [{'classname': 'team_control_point', 'point_default_owner': str(owner)} for owner in ([2,0,3] if count == 3 else [2,2,0,3,3])]
            self.assertEqual(mod.classify('cp_dustbowl_variant', points)[0], 'control points')

    def test_ad_variants_and_live_asymmetry(self):
        for name in ('cp_gravelpit_snowy', 'cp_dustbowl_pro_b1', 'workshop/cp_steel_trad.ugc123'):
            self.assertEqual(mod.classify(name, [])[0], 'attack/defend')
        es = [{'classname':'team_control_point', 'point_default_owner':'2'}, {'classname':'team_control_point_master','cpm_restrict_team_cap_win':'2'}]
        self.assertEqual(mod.classify('cp_unknown', es)[0], 'attack/defend')

    def test_sulfur_hybrid_and_snowplow_are_ad(self):
        es = [{'classname':'team_control_point', 'point_default_owner':'2'}, {'classname':'team_control_point_master','cpm_restrict_team_cap_win':'2'}]
        self.assertEqual(mod.classify('cp_sulfur', es+[{'classname':'tf_logic_koth'}])[0], 'attack/defend')
        self.assertEqual(mod.classify('cp_snowplow', es+[{'classname':'team_train_watcher','train':'train'}])[0], 'attack/defend')

    def test_payload_is_cart_capture_not_any_train(self):
        es = [{'classname':'team_train_watcher','train':'cart'}, {'classname':'trigger_capture_area','parentname':'cart'}]
        self.assertEqual(mod.classify('cp_misnamed', es)[0], 'payload')

    def test_real_koth_and_scripted_cp_restriction(self):
        self.assertEqual(mod.classify('cp_named_wrong', [{'classname':'tf_logic_koth'}])[0], 'koth')
        es = [{'classname':'team_control_point_master','cpm_restrict_team_cap_win':'1'}, {'classname':'team_control_point','point_default_owner':'2'}, {'classname':'team_control_point','point_default_owner':'3'}]
        self.assertEqual(mod.classify('cp_canaveral_5cp', es)[0], 'control points')


if __name__ == '__main__':
    unittest.main()
